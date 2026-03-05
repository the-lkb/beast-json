// bench_ser_profile.cpp — serialize-only hot loop for perf profiling.
// Parse once, then dump() in a tight loop with NO parse in the loop.
// Build: g++ -O3 -march=native -g -fno-omit-frame-pointer -std=c++20
//            -I../include bench_ser_profile.cpp -o bench_ser_profile
// Run:   perf record -g ./bench_ser_profile citm_catalog.json 5000
//        perf report --stdio

#include <beast_json/beast_json.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

static std::string read_file(const char *path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
  auto sz = f.tellg(); f.seekg(0);
  std::string buf(sz, '\0');
  f.read(&buf[0], sz);
  return buf;
}

int main(int argc, char **argv) {
  const char *file = (argc > 1) ? argv[1] : "citm_catalog.json";
  int N = (argc > 2) ? atoi(argv[2]) : 5000;

  std::string src = read_file(file);
  fprintf(stderr, "file: %s  size: %.1f KB  iters: %d\n",
          file, src.size() / 1024.0, N);

  // Parse once — outside the hot loop
  beast::Document ctx;
  auto doc = beast::parse(ctx, src);

  // Baseline: alloc-every-iter (old behaviour)
  using clock = std::chrono::high_resolution_clock;
  for (int i = 0; i < 200; ++i) (void)doc.dump();
  auto t0 = clock::now();
  for (int i = 0; i < N; ++i)  (void)doc.dump();
  auto t1 = clock::now();
  double us_alloc = std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
  fprintf(stderr, "old (alloc-per-iter):   %.2f μs\n", us_alloc);

  // Phase 66-B: reuse buffer
  std::string reuse_buf;
  reuse_buf.reserve(src.size() + 16);
  for (int i = 0; i < 200; ++i) doc.dump(reuse_buf);
  t0 = clock::now();
  for (int i = 0; i < N; ++i) doc.dump(reuse_buf);
  t1 = clock::now();

  double us = std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
  double mb_s = (src.size() / 1e6) / (us / 1e6);
  fprintf(stderr, "new (reuse-buffer):     %.2f μs/iter  %.2f GB/s\n", us, mb_s / 1e3);
  fprintf(stderr, "improvement: %.1f%%\n", (us_alloc - us) / us_alloc * 100.0);
  return 0;
}
