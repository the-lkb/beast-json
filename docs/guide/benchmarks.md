# Performance Benchmarks

<div style="display: flex; flex-wrap: wrap; gap: 0.4rem; margin: 0.75rem 0 1.5rem; line-height: 1.9;">
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml/badge.svg" alt="CI" /></a>
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml/badge.svg" alt="Benchmark CI" /></a>
  <img src="https://img.shields.io/badge/data-open%20source-blue" alt="Open benchmark data" />
  <img src="https://img.shields.io/badge/platforms-x86__64%20%7C%20aarch64%20%7C%20Apple%20Silicon-lightgrey" alt="Platforms" />
</div>

All benchmark data is generated automatically on every push to `main` that touches `include/` or `benchmarks/`.
Runs in parallel on three native GitHub-hosted runners (x86\_64 / Apple Silicon / Linux AArch64).

> **All benchmark data is open.**  The raw JSON results file is committed to the repository at
> [`docs/public/benchmark-results.json`](https://github.com/qbuem/qbuem-json/blob/main/docs/public/benchmark-results.json)
> and the [workflow that produces it](https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml)
> is fully public.  You can run the exact same suite yourself — see
> [Reproducing our benchmarks](#reproducing-our-benchmarks) below.

| Runner | Architecture | Compiler |
|:---|:---|:---|
| `ubuntu-latest` | x86_64 | GCC 13, Release |
| `ubuntu-24.04-arm` | Linux aarch64 | GCC 14, Release |
| `macos-latest` | Apple Silicon | Apple Clang, Release |

<BenchmarkCi />

---

## 🔗 Language Bindings Performance

`qbuem-json` provides high-performance bindings for **Python**, **Go**, and **Rust**. The automated benchmark dashboard above includes a dedicated section for these bindings, comparing them against the native JSON libraries in each language.

---

## 🔗 Object Mapping (Nexus Fusion)

Unlike DOM-based parsing, **Nexus Fusion** maps JSON directly to C++ structs. These benchmarks measure the latency of full deserialization into complex STL types.

> [!TIP]
> Use the "Dataset" selector above to switch between Standard DOM (twitter.json, etc.) and Object Mapping (Simple Object, Extreme Harsh STL) results.

---

## ⚙️ Architecture-Specific Optimizations

| Architecture | Technique | Benefit |
|:---|:---|:---|
| **x86_64** | AVX-512 64B/iter Stage 1 scan | Extracts structural tokens in a single pass, ahead of parsing |
| **x86_64** | BMI2 `PEXT`/`PDEP` | Bit-manipulation for separator extraction without branches |
| **Apple Silicon** | NEON 16B vectorized bitmasks | Branchless character detection tuned for 128-byte M-series cache lines |
| **All** | SWAR (SIMD Within A Register) | 8 bytes/cycle string scan using 64-bit GPR — no SIMD required for short strings |
| **All** | KeyLenCache | O(1) key lookup for repeated object schemas (e.g., JSON arrays of same-shape objects) |
| **All** | Schubfach dtoa (R. Giulietti / yyjson port) | Shortest round-trip double→decimal — 2–3× faster than `std::to_chars(double)`, no trailing zeros |
| **All** | yy-itoa (Y. Yuan / yyjson port) | Integer→decimal via 2-digit table + multiply-shift — zero division instructions |
| **All** | Russ Cox Fast Unrounded Scaling ([Cox 2026](https://research.swtch.com/fp)) | Stage-2 parser: ceiling of table high word (`ph_ceil = ph + (pl≠0)`) makes sticky bit always decisive — resolves the ~1.2 % of cases Eisel-Lemire cannot, without `strtod` |


---

## Reproducing our benchmarks

The benchmark suite is open source and you can run it on your own hardware in
under 10 minutes.

### Prerequisites

| Requirement | Notes |
|:---|:---|
| Linux x86_64 or macOS Apple Silicon | Windows is not yet supported for benchmarks |
| GCC 13+ or Clang 18+ | Must support C++20 and `-march=native` |
| CMake 3.21+ | For FetchContent and presets support |
| ~500 MB disk | Benchmark datasets (downloaded automatically) |

### Step-by-step

```bash
# 1. Clone
git clone https://github.com/qbuem/qbuem-json.git
cd qbuem-json

# 2. Download the standard benchmark datasets from simdjson-data
mkdir -p bench_data
for file in twitter.json canada.json citm_catalog.json gsoc-2018.json; do
  curl -fsSL \
    "https://raw.githubusercontent.com/simdjson/simdjson-data/master/jsonexamples/${file}" \
    -o "bench_data/${file}"
done

# 3. Configure — point CMake at the pre-downloaded data directory
cmake -B build_bench \
  -DCMAKE_BUILD_TYPE=Release \
  -DQBUEM_JSON_BUILD_TESTS=OFF \
  -DQBUEM_JSON_BUILD_BENCHMARKS=ON \
  -DCMAKE_CXX_FLAGS="-march=native" \
  -DBENCHMARK_DATA_PREDOWNLOAD_DIR="$(pwd)/bench_data"

# 4. Build
cmake --build build_bench -j$(nproc)

# 5. Copy data files next to the binaries (bench_all re-invokes itself per library)
cp bench_data/*.json build_bench/benchmarks/

# 6. Run
cd build_bench/benchmarks
./bench_all --all       # DOM parsing + serialisation
./bench_structs --all   # Struct-mapping (Nexus Fusion)
```

### Benchmark methodology

| Parameter | Value |
|:---|:---|
| Measurement | Wall-clock time, `std::chrono::steady_clock`, nanosecond resolution |
| Warmup | 5 dry runs before timing |
| Iterations | 100 timed iterations per dataset/library pair |
| Reported value | **Median** of 100 runs (robust against transient OS jitter) |
| CPU affinity | `taskset -c 0` pins to a single core; turbo boost left as-is |
| Allocator | System allocator (`malloc`); no jemalloc or tcmalloc |
| Compiler flags | `-O3 -march=native -DNDEBUG` (same for all libraries) |
| Memory | Allocation measured with a `std::pmr::monotonic_buffer_resource` wrapper |

### Competing library versions tested

| Library | Pinned version | Source |
|:---|:---|:---|
| simdjson | v3.10.1 | FetchContent from GitHub |
| yyjson | 0.10.0 | FetchContent from GitHub |
| RapidJSON | master HEAD | FetchContent from GitHub |
| Glaze | latest (C++23, optional) | FetchContent from GitHub |
| nlohmann/json | v3.11.3 | FetchContent tarball |

All libraries are compiled from source with the same `-O3 -march=native` flags.
The one exception: `yyjson` also receives `-march=native` explicitly via
`target_compile_options` to ensure it can use AVX2/AVX-512 on x86_64 — the
same hardware advantage qbuem-json uses.  This makes the comparison fair.

### Frequently asked questions

**Why is simdjson sometimes faster on twitter.json parse?**

simdjson uses a two-pass architecture that defers validation to a lazy step,
allowing Stage 1 to run without any structural bookkeeping.  qbuem-json's DOM
engine produces a fully indexed tape in one pass, which pays a small overhead
for the generality of O(1) random access.  On streaming/iterator workloads that
don't need random access, simdjson's deferred model wins on parse throughput.
qbuem-json leads on total parse+access+serialise latency for typical use cases.

**Why is the harsh.json serialize time higher for some libraries?**

`harsh.json` is a synthetically generated worst-case dataset with extremely deep
nesting, long strings with many escape sequences, and a high proportion of
floating-point values.  Serialiser throughput on this dataset is limited by
`double`→decimal conversion speed; libraries using `snprintf` or `std::to_chars`
fall significantly behind Schubfach.

**Can I add my own library to the comparison?**

Yes.  The benchmark harness in `benchmarks/` uses a plugin interface.  Open a
pull request with an adapter in `benchmarks/adapters/` following the existing
pattern.
