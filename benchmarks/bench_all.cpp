// benchmarks/bench_all.cpp
// Unified benchmark: qbuem-json (DOM) vs yyjson vs nlohmann/json
// Measures parse + serialize throughput on 4 standard JSON files.
//
// Usage:
//   ./bench_all [file.json]              # single file (default: twitter.json)
//   ./bench_all --all                    # run all 4 standard files
//   sequentially
//   ./bench_all --parse-only --all       # PGO GENERATE: parse paths only

#include "utils.hpp"
#include <qbuem_json/qbuem_json.hpp>
#include <fstream>
#ifdef QBUEM_HAS_GLAZE
#include <glaze/glaze.hpp>
#endif
#include <nlohmann/json.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <simdjson.h>
#include <yyjson.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// ── Benchmark one file ─────────────────────────────────────────────────────

static void run_file(const std::string &exe_path, const std::string &lib_filter,
                     const std::string &filename, size_t N, bool parse_only) {
  if (lib_filter.empty()) {
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

    std::vector<std::string> libs = {"qbuem-json (DOM)", "simdjson",  "yyjson",
                                     "RapidJSON",   "Glaze DOM", "nlohmann"};
    for (const auto &lib : libs) {
#ifndef QBUEM_HAS_GLAZE
      if (lib == "Glaze DOM")
        continue;
#endif
      std::string cmd = exe_path + " " + filename + " --iter " +
                        std::to_string(N) + " --lib \"" + lib + "\"";
      if (parse_only)
        cmd += " --parse-only";
      std::cout << std::flush;
      int ret = system(cmd.c_str());
      (void)ret;
    }
    std::cout << "\n";
    return;
  }

  std::string content;
  try {
    content = bench::read_file(filename.c_str());
  } catch (...) {
    return;
  }

  // ── 1. qbuem-json (DOM) (production path) ──────────────────────────────
  if (lib_filter == "qbuem-json (DOM)") {
    // Memory: RSS before vs after cold parse — measures tape arena size
    size_t rss0 = bench::get_current_rss_kb();
    qbuem::Document ctx;
    qbuem::parse(ctx, content); // cold parse: allocates and sizes the tape
    size_t rss1 = bench::get_current_rss_kb();
    size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < N; ++i)
      qbuem::parse(ctx, content);
    double p_ns = pt.elapsed_ns() / N;

    double s_ns = 0.0;
    bool ok = true;
    auto doc = qbuem::parse(ctx, content);

    if (!parse_only) {
      std::string dump_buf;
      st.start();
      for (size_t i = 0; i < N; ++i)
        doc.dump(dump_buf);
      s_ns = st.elapsed_ns() / N;

      std::string out = doc.dump();
      ok = !out.empty();
    }

    bench::Result{"qbuem-json (DOM)", p_ns, s_ns, ok, alloc_kb}.print();
  }

  // ── 1.5 simdjson ─────────────────────────────────────────────────────────
  if (lib_filter == "simdjson") {
    // Memory: padded input copy + parser internal tape
    size_t rss0 = bench::get_current_rss_kb();
    simdjson::padded_string padded(content);
    simdjson::dom::parser parser;
    auto doc_mem = parser.parse(padded); // cold parse: sizes internal buffers
    (void)doc_mem;
    size_t rss1 = bench::get_current_rss_kb();
    size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < N; ++i) {
      auto doc = parser.parse(padded);
      (void)doc;
    }
    double p_ns = pt.elapsed_ns() / N;

    double s_ns = 0.0;
    if (!parse_only) {
      auto doc = parser.parse(padded);
      std::string out;
      st.start();
      for (size_t i = 0; i < N; ++i) {
        out = simdjson::minify(doc);
      }
      s_ns = st.elapsed_ns() / N;
    }

    bench::Result{"simdjson", p_ns, s_ns, true, alloc_kb}.print();
  }

  // ── 2. yyjson ────────────────────────────────────────────────────────────
  if (lib_filter == "yyjson") {
    // Memory: one cold parse, document kept live while measuring
    size_t rss0 = bench::get_current_rss_kb();
    yyjson_doc *d_mem = yyjson_read(content.c_str(), content.size(), 0);
    size_t rss1 = bench::get_current_rss_kb();
    size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;
    yyjson_doc_free(d_mem);

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

    bench::Result{"yyjson", p_ns, s_ns, true, alloc_kb}.print();
  }

  // ── 3. RapidJSON ─────────────────────────────────────────────────────────
  if (lib_filter == "RapidJSON") {
    // Memory: one cold parse, document kept live while measuring
    size_t rss0 = bench::get_current_rss_kb();
    {
      rapidjson::Document d_mem;
      d_mem.Parse(content.c_str());
      size_t rss1 = bench::get_current_rss_kb();
      size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

      bench::Timer pt, st;
      pt.start();
      for (size_t i = 0; i < N; ++i) {
        rapidjson::Document d;
        d.Parse(content.c_str());
      }
      double p_ns = pt.elapsed_ns() / N;

      double s_ns = 0.0;
      if (!parse_only) {
        rapidjson::Document d;
        d.Parse(content.c_str());
        rapidjson::StringBuffer buffer;
        st.start();
        for (size_t i = 0; i < N; ++i) {
          buffer.Clear();
          rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
          d.Accept(writer);
        }
        s_ns = st.elapsed_ns() / N;
      }

      bench::Result{"RapidJSON", p_ns, s_ns, true, alloc_kb}.print();
    }
  }

  // ── 4. Glaze (DOM glz::json_t) ───────────────────────────────────────────
#ifdef QBUEM_HAS_GLAZE
  if (lib_filter == "Glaze DOM") {
    // Memory: one cold parse, document kept live while measuring
    size_t rss0 = bench::get_current_rss_kb();
    {
      glz::json_t d_mem;
      (void)glz::read_json(d_mem, content);
      size_t rss1 = bench::get_current_rss_kb();
      size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

      bench::Timer pt, st;
      pt.start();
      for (size_t i = 0; i < N; ++i) {
        glz::json_t d;
        auto ec = glz::read_json(d, content);
        (void)ec;
      }
      double p_ns = pt.elapsed_ns() / N;

      double s_ns = 0.0;
      if (!parse_only) {
        glz::json_t d;
        (void)glz::read_json(d, content);
        std::string out;
        st.start();
        for (size_t i = 0; i < N; ++i) {
          out.clear();
          (void)glz::write_json(d, out);
        }
        s_ns = st.elapsed_ns() / N;
      }

      bench::Result{"Glaze DOM", p_ns, s_ns, true, alloc_kb}.print();
    }
  }
#endif

  // ── 5. nlohmann/json (baseline) ──────────────────────────────────────────
  if (lib_filter == "nlohmann") {
    // Memory: one cold parse, document kept live while measuring
    size_t rss0 = bench::get_current_rss_kb();
    {
      [[maybe_unused]] auto j_mem = nlohmann::json::parse(content);
      size_t rss1 = bench::get_current_rss_kb();
      size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

      bench::Timer pt, st;
      pt.start();
      for (size_t i = 0; i < N; ++i) {
        [[maybe_unused]] auto j = nlohmann::json::parse(content);
      }
      double p_ns = pt.elapsed_ns() / N;

      double s_ns = 0.0;
      if (!parse_only) {
        nlohmann::json j = nlohmann::json::parse(content);
        st.start();
        for (size_t i = 0; i < N; ++i)
          (void)j.dump();
        s_ns = st.elapsed_ns() / N;
      }

      bench::Result{"nlohmann", p_ns, s_ns, true, alloc_kb}.print();
    }
  }
}

// ── main ──────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
  size_t N = 300;
  int file_arg = -1;
  bool run_all = false;
  bool parse_only = false;
  bool quick_mode = false;
  std::string lib_filter = "";
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--iter") == 0 && i + 1 < argc) {
      N = static_cast<size_t>(std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
      lib_filter = argv[++i];
    } else if (std::strcmp(argv[i], "--all") == 0) {
      run_all = true;
    } else if (std::strcmp(argv[i], "--parse-only") == 0) {
      parse_only = true;
    } else if (std::strcmp(argv[i], "--quick") == 0) {
      quick_mode = true;
    } else {
      file_arg = i;
    }
  }

  if (run_all) {
    // Generate a harsh environment JSON (heavy escapes, nesting, floats)
    if (lib_filter.empty()) {
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
    size_t scale = quick_mode ? 20 : 1;
    for (const auto &f : files)
      run_file(argv[0], lib_filter, f, std::max<size_t>(1, N / scale),
               parse_only);
  } else {
    size_t scale = quick_mode ? 20 : 1;
    const std::string filename =
        (file_arg >= 0) ? argv[file_arg] : "twitter.json";
    run_file(argv[0], lib_filter, filename, std::max<size_t>(1, N / scale),
             parse_only);
  }
  return 0;
}
