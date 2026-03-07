// benchmarks/bench_all.cpp
// Unified benchmark: beast::json::lazy vs yyjson vs nlohmann/json
// Measures parse + serialize throughput on 4 standard JSON files.
//
// Usage:
//   ./bench_all [file.json]              # single file (default: twitter.json)
//   ./bench_all --all                    # run all 4 standard files
//   sequentially
//   ./bench_all --parse-only --all       # PGO GENERATE: parse paths only

#include "utils.hpp"
#include <beast_json/beast_json.hpp>
#include <fstream>
#include <glaze/glaze.hpp>
#include <nlohmann/json.hpp>
#include <yyjson.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// ── Benchmark one file ─────────────────────────────────────────────────────

static void run_file(const std::string &filename, size_t N, bool parse_only) {
  std::string content;
  try {
    content = bench::read_file(filename.c_str());
  } catch (const std::exception &e) {
    std::cerr << "Skip " << filename << ": " << e.what() << "\n";
    return;
  }
  if (content.empty()) {
    std::cerr << "Skip " << filename << ": empty\n";
    return;
  }

  bench::print_header("bench_all — " + filename);
  std::cout << "Size: " << (content.size() / 1024.0) << " KB"
            << "  Iterations: " << N << "\n";
  bench::print_table_header();

  // ── 1. beast::json::lazy (production path) ──────────────────────────────
  {
    beast::Document ctx;
    // Warm-up: size the tape
    beast::parse(ctx, content);

    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < N; ++i)
      beast::parse(ctx, content);
    double p_ns = pt.elapsed_ns() / N;

    double s_ns = 0.0;
    bool ok = true;
    auto doc = beast::parse(ctx, content);

    if (!parse_only) {
      // Phase 73: use buffer-reuse dump(string&) — amortises malloc+memset.
      std::string dump_buf;
      st.start();
      for (size_t i = 0; i < N; ++i)
        doc.dump(dump_buf);
      s_ns = st.elapsed_ns() / N;

      // Correctness: round-trip via nlohmann comparison
      std::string out = doc.dump();
      try {
        ok = (nlohmann::json::parse(content) == nlohmann::json::parse(out));
      } catch (...) {
        ok = false;
      }
      if (!ok)
        std::cerr << "  beast::lazy verify FAIL (snippet: " << out.substr(0, 80)
                  << "...)\n";
    }

    bench::Result{"beast::lazy", p_ns, s_ns, ok}.print();
  }

  // ── 2. yyjson ────────────────────────────────────────────────────────────
  {
    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < N; ++i) {
      yyjson_doc *d = yyjson_read(content.c_str(), content.size(), 0);
      yyjson_doc_free(d);
    }
    double p_ns = pt.elapsed_ns() / N;

    double s_ns = 0.0;
    if (!parse_only) {
      yyjson_doc *d = yyjson_read(content.c_str(), content.size(), 0);
      st.start();
      for (size_t i = 0; i < N; ++i) {
        size_t l;
        char *s = yyjson_write(d, 0, &l);
        free(s);
      }
      s_ns = st.elapsed_ns() / N;
      yyjson_doc_free(d);
    }

    bench::Result{"yyjson", p_ns, s_ns, true}.print();
  }

  // ── 3. Glaze (DOM glz::json_t) ───────────────────────────────────────────
  {
    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < N; ++i) {
      glz::json_t d;
      auto ec = glz::read_json(d, content);
      if (ec) {
        // just ignore errors for bench timing
      }
    }
    double p_ns = pt.elapsed_ns() / N;

    double s_ns = 0.0;
    if (!parse_only) {
      glz::json_t d;
      glz::read_json(d, content);
      std::string out;
      st.start();
      for (size_t i = 0; i < N; ++i) {
        out.clear();
        glz::write_json(d, out);
      }
      s_ns = st.elapsed_ns() / N;
    }

    bench::Result{"Glaze DOM", p_ns, s_ns, true}.print();
  }

  // ── 4. nlohmann/json (baseline) ──────────────────────────────────────────
  {
    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < N; ++i)
      (void)nlohmann::json::parse(content);
    double p_ns = pt.elapsed_ns() / N;

    double s_ns = 0.0;
    if (!parse_only) {
      nlohmann::json j = nlohmann::json::parse(content);
      st.start();
      for (size_t i = 0; i < N; ++i)
        (void)j.dump();
      s_ns = st.elapsed_ns() / N;
    }

    bench::Result{"nlohmann", p_ns, s_ns, true}.print();
  }

  std::cout << "\n";
}

// ── main ──────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
  size_t N = 300;

  // Parse arguments: [--iter <N>] [--parse-only] [--all | file.json]
  //
  // --parse-only: skip serialize loops.  Use during PGO GENERATE so only
  //   parse code paths are profiled → LTO optimises for parse I-cache layout.
  //   Example PGO workflow (GCC):
  //     cmake -DPGO_MODE=GENERATE && ninja
  //     cd build_bench/benchmarks && ./bench_all --parse-only --iter 30 --all
  //     cmake -DPGO_MODE=USE && ninja
  int file_arg = -1;
  bool run_all = false;
  bool parse_only = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--iter") == 0 && i + 1 < argc) {
      N = static_cast<size_t>(std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--all") == 0) {
      run_all = true;
    } else if (std::strcmp(argv[i], "--parse-only") == 0) {
      parse_only = true;
    } else {
      file_arg = i;
    }
  }

  if (run_all) {
    // Generate a harsh environment JSON (heavy escapes, nesting, floats)
    {
      std::ifstream ifs("harsh.json");
      if (!ifs.is_open()) {
        std::ofstream ofs("harsh.json");
        std::string s = "{\n";
        for (int i = 0; i < 50000; ++i) {
          s += "\"harsh_\\\"escaped\\\\_" + std::to_string(i) + "\": ";
          s += "[ 12345.6789e-10, true, null, false, "
               "\"nested\\n\\t\\rstring\", { \"deep\": [1,2,3,4,5] } ]";
          if (i < 49999)
            s += ",\n";
        }
        s += "\n}";
        ofs << s;
      }
    }

    const std::vector<std::string> files = {"twitter.json", "canada.json",
                                            "citm_catalog.json",
                                            "gsoc-2018.json", "harsh.json"};
    for (const auto &f : files)
      run_file(f, N, parse_only);
  } else {
    const std::string filename =
        (file_arg >= 0) ? argv[file_arg] : "twitter.json";
    run_file(filename, N, parse_only);
  }
  return 0;
}
