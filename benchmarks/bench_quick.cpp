// benchmarks/bench_quick.cpp
// Beast::json lazy parser — parse + dump microbenchmark.
// Only depends on beast_json (no third-party libs), so it is
// safe to run on Android kernels that do not expose SVE.
//
// Usage (from build_bench/benchmarks/):
//   ./bench_quick              # twitter.json only, 300 iter
//   ./bench_quick --all        # all 4 standard files
//   ./bench_quick --iter 500   # custom iteration count

#include <beast_json/beast_json.hpp>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string read_file(const char *path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static double measure_parse(beast::Document &ctx,
                             const std::string &content, int N) {
    for (int i = 0; i < 20; ++i)
        beast::parse(ctx, content);
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        beast::parse(ctx, content);
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
}

static double measure_dump(beast::Document &ctx,
                            const std::string &content, int N) {
    beast::parse(ctx, content);
    auto root = beast::parse(ctx, content);
    // Phase 73: buffer-reuse overload — pre-allocate once, skip malloc+memset.
    std::string out;
    out.reserve(content.size() + 16);
    for (int i = 0; i < 20; ++i)
        root.dump(out);
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        root.dump(out);
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
}

int main(int argc, char **argv) {
    // Paths are relative to the binary's working directory.
    // JSON files are downloaded into build_bench/benchmarks/ by CMake.
    const char *files[] = {
        "twitter.json",
        "canada.json",
        "citm_catalog.json",
        "gsoc-2018.json",
    };
    bool all = false;
    int N = 300;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--all") == 0) all = true;
        if (strcmp(argv[i], "--iter") == 0 && i + 1 < argc) N = atoi(argv[++i]);
    }

    printf("Iterations: %d\n", N);
    printf("%-30s %10s %10s\n", "file", "parse(us)", "dump(us)");
    printf("%-30s %10s %10s\n", "----", "---------", "--------");

    int nfiles = all ? 4 : 1;
    for (int fi = 0; fi < nfiles; ++fi) {
        const char *f = files[fi];
        std::string content = read_file(f);
        if (content.empty()) {
            std::cerr << "Skip " << f << " (not found)\n";
            continue;
        }
        beast::Document ctx;
        double p = measure_parse(ctx, content, N);
        double d = measure_dump(ctx, content, N);
        printf("%-30s %10.1f %10.1f\n", f, p, d);
    }
}
