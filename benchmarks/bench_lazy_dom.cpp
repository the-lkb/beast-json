// benchmarks/bench_lazy_dom.cpp
// Phase 17: Lazy-Manifest DOM benchmark
// Compares beast::json::lazy (Tape IS the DOM) vs yyjson vs nlohmann
#include "utils.hpp"
#include <beast_json/beast_json.hpp>
#include <nlohmann/json.hpp>
#include <yyjson.h>

int main(int argc, char **argv) {
  const char *filename = (argc > 1) ? argv[1] : "twitter.json";
  std::string json_content = bench::read_file(filename);
  if (json_content.empty())
    return 1;

  const size_t N = 500;
  bench::print_header("Phase 17: Lazy-Manifest DOM Benchmark");
  std::cout << "File: " << filename << " (" << (json_content.size() / 1024.0)
            << " KB)\n";
  bench::print_table_header();

  // 1. beast::json::lazy — Zero-cost Lazy-Manifest DOM
  {
    // Pre-allocate a persistent DocumentView (tape buffer reused across
    // iterations)
    std::string_view sv{json_content};
    beast::Document ctx{sv};
    // Warm-up: parse once to size the tape
    beast::parse(ctx, json_content);

    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < N; ++i) {
      beast::parse(ctx, json_content);
    }
    double p_ns = pt.elapsed_ns() / N;

    auto doc = beast::parse(ctx, json_content);
    st.start();
    for (size_t i = 0; i < N; ++i) {
      (void)doc.dump();
    }
    double s_ns = st.elapsed_ns() / N;

    // round-trip verify
    std::string out = doc.dump();
    bool ok = false;
    try {
      ok = (nlohmann::json::parse(json_content) == nlohmann::json::parse(out));
    } catch (...) {
    }
    if (!ok) {
      std::cout << "Verify FAIL — output snippet: " << out.substr(0, 120)
                << "...\n";
    }

    bench::Result r{"beast::lazy", p_ns, s_ns, ok};
    r.print();
  }

  // 2. yyjson — world's current fastest
  {
    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < N; ++i) {
      yyjson_doc *d = yyjson_read(json_content.c_str(), json_content.size(), 0);
      yyjson_doc_free(d);
    }
    double p_ns = pt.elapsed_ns() / N;

    yyjson_doc *d = yyjson_read(json_content.c_str(), json_content.size(), 0);
    st.start();
    for (size_t i = 0; i < N; ++i) {
      size_t l;
      char *s = yyjson_write(d, 0, &l);
      free(s);
    }
    double s_ns = st.elapsed_ns() / N;
    yyjson_doc_free(d);

    bench::Result r{"yyjson", p_ns, s_ns, true};
    r.print();
  }

  // 3. nlohmann — baseline
  {
    bench::Timer pt, st;
    pt.start();
    for (size_t i = 0; i < N; ++i) {
      nlohmann::json::parse(json_content);
    }
    double p_ns = pt.elapsed_ns() / N;

    nlohmann::json j = nlohmann::json::parse(json_content);
    st.start();
    for (size_t i = 0; i < N; ++i) {
      (void)j.dump();
    }
    double s_ns = st.elapsed_ns() / N;

    bench::Result r{"nlohmann", p_ns, s_ns, true};
    r.print();
  }

  return 0;
}
