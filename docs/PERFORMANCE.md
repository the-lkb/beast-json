# Beast JSON — Performance Guide

> All measurements taken on dedicated bare-metal hardware. CI runners are **never** used for performance measurements due to scheduler noise. Benchmark files: `twitter.json` (617 KB), `canada.json` (2.2 MB), `citm_catalog.json` (1.7 MB), `gsoc-2018.json` (3.2 MB).

## Build Configuration Note

All Beast JSON numbers in this document use **PGO (Profile-Guided Optimization) + LTO (Link-Time Optimization)**. This is important context when comparing against other libraries:

| Flag | What it does | Effect on numbers |
|:---|:---|:---|
| `-O3` | Aggressive inlining, loop unrolling, vectorization | Baseline; all libraries use this |
| `-march=native` | Enable all CPU-native ISA extensions (AVX-512, NEON, etc.) | Enables SIMD intrinsics; yyjson also compiled with this |
| `-flto` | Link-Time Optimization — whole-program analysis across TUs | Inlines across translation-unit boundaries; enables dead-code elimination of cold paths |
| **PGO** (parse-only) | GCC `-fprofile-generate` → training run → `-fprofile-use` | Branch-prediction hints, basic-block reordering, inlining decisions guided by actual parse workload |

**Parse-only PGO is critical:** Using a mixed parse+serialize PGO profile causes LTO to interleave serialize code into parse hot paths, degrading parse performance by ~10% on citm_catalog. Beast uses a **parse-only training workload** (Phase 75A) to maximize parse throughput at the cost of slightly suboptimal serialize branch hints.

**yyjson comparison:** yyjson is compiled with `-O3 -march=native` but **without PGO**. This is yyjson at its standard best-effort build — the comparison is fair in the sense that yyjson does not publish PGO-tuned numbers, and most users will not apply PGO to yyjson.

For reproduction instructions including the two-step PGO build: see [How to Reproduce](#how-to-reproduce).

---

## Table of Contents

1. [Benchmark Results by Platform](#benchmark-results-by-platform)
   - [Linux x86_64 — Phase 75](#linux-x86_64--phase-75)
   - [Apple M1 Pro — Phase 80-M1](#apple-m1-pro--phase-80-m1)
   - [Snapdragon 8 Gen 2 — Phase 73](#snapdragon-8-gen-2--phase-73)
2. [Competitive Comparison](#competitive-comparison)
3. [Benchmark Files — Characteristics](#benchmark-files--characteristics)
4. [How to Reproduce](#how-to-reproduce)
5. [Optimization Phase Summary](#optimization-phase-summary)
6. [Understanding the Numbers](#understanding-the-numbers)
7. [Performance Tuning Knobs](#performance-tuning-knobs)
8. [Known Limits](#known-limits)

---

## Benchmark Results by Platform

### Linux x86_64 — Phase 75

**Environment:**
- CPU: Intel (AVX-512 capable), GCC 13.3.0
- Flags: `-O3 -flto -march=native` + **parse-only PGO** (two-step: `-fprofile-generate` training run → `-fprofile-use -fprofile-correction`)
- yyjson: `-O3 -flto -march=native`, no PGO
- Iterations: 300 per file, dedicated bare-metal (not CI)
- yyjson compiled with full SIMD enabled (`-march=native`)

#### Parse throughput

| File | Beast (μs) | Beast (GB/s) | yyjson (μs) | yyjson (GB/s) | Beast advantage |
|:---|---:|---:|---:|---:|:---:|
| twitter.json | **189** | **3.27 GB/s** | 282 | 2.19 GB/s | **+49%** ✅ |
| canada.json | **1,433** | **1.54 GB/s** | 2,595 | 0.85 GB/s | **+81%** ✅ |
| citm_catalog.json | **626** | **2.70 GB/s** | 757 | 2.23 GB/s | **+21%** ✅ |
| gsoc-2018.json | **731** | **4.45 GB/s** | 1,615 | 2.01 GB/s | **+121%** ✅ |

#### Serialize throughput

| File | Beast (μs) | yyjson (μs) | Beast advantage |
|:---|---:|---:|:---:|
| twitter.json | **145** | 131 | yyjson 9% faster |
| canada.json | **789** | 3,301 | **Beast 4.18×** ✅ |
| citm_catalog.json | **312** | 235 | yyjson 33% faster |
| gsoc-2018.json | **369** | 1,417 | **Beast 3.84×** ✅ |

> **Status (Phase 75):** Parse 4/4 ✅. Serialize 2/4 beating yyjson by ≥1.2×; citm and twitter serialize still trail (citm narrowed from 49% gap to 33% via Phase 73+75B).

---

### Apple M1 Pro — Phase 80-M1

**Environment:**
- CPU: Apple M1 Pro
- OS: macOS 26.3, Apple Clang 17
- Flags: `-O3 -march=native` + **LLVM PGO** (`-fprofile-generate` → `llvm-profdata merge` → `-fprofile-instr-use`) + implicit LTO via Apple's linker
- yyjson: `-O3 -march=native`, no PGO
- Iterations: 500 per file

#### Parse throughput

| File | Beast (μs) | yyjson (μs) | Result |
|:---|---:|---:|:---:|
| twitter.json | 205 | **170** | yyjson 21% faster |
| canada.json | 1,501 | **1,432** | yyjson 5% faster |
| citm_catalog.json | 534 | **462** | yyjson 16% faster |
| gsoc-2018.json | **572** | 966 | Beast **69%** faster ✅ |

#### Serialize throughput

| File | Beast (μs) | yyjson (μs) | Result |
|:---|---:|---:|:---:|
| twitter.json | **66** | 102 | **Beast +35%** ✅ |
| canada.json | **701** | 2,210 | **Beast 3.15×** ✅ |
| citm_catalog.json | **166** | 164 | **Tied** (1.2% = noise) ✅ |
| gsoc-2018.json | **169** | 703 | **Beast 4.2×** ✅ |

> **Status (Phase 80-M1):** Serialize 4/4 ✅ (citm effectively tied). Parse lags for 3/4 files — root cause is M1's 576-entry ROB favoring yyjson's flat-array iterator over Beast's tape-indirection model. Architectural limit; see [Known Limits](#known-limits).

---

### Snapdragon 8 Gen 2 — Phase 73

**Environment:**
- Device: Samsung Galaxy Z Fold 5
- OS: Android, Termux, Clang 21.1.8
- Flags: `-O3 -march=armv8.4-a+crypto+dotprod+fp16+i8mm+bf16` (no SVE — kernel disabled); **no PGO** (Android Termux profiling infrastructure unavailable at time of measurement)
- yyjson: same flags, no PGO
- CPU pinned to cpu6 (Cortex-A715 Gold, 2803 MHz); prime core (Cortex-X3 3360 MHz) offline at measurement time
- Iterations: 300 per file, dedicated device (not CI)

#### Parse throughput

| File | Beast (μs) | Beast (GB/s) | yyjson (μs) | Beast advantage |
|:---|---:|---:|---:|:---:|
| twitter.json | **321.8** | **1.92 GB/s** | 536.9 | **+67%** ✅ |
| canada.json | **2,103.5** | **1.05 GB/s** | 3,614.8 | **+72%** ✅ |
| citm_catalog.json | **844.1** | **2.00 GB/s** | 1,627.2 | **+93%** ✅ |
| gsoc-2018.json | **956.4** | **3.40 GB/s** | 2,421.7 | **+153%** ✅ |

#### Serialize throughput

| File | Beast (μs) | yyjson (μs) | Beast advantage |
|:---|---:|---:|:---:|
| twitter.json | **121.3** | 264.1 | **2.2×** ✅ |
| canada.json | **676.5** | 3,774.7 | **5.6×** ✅ |
| citm_catalog.json | **325.5** | 754.2 | **2.3×** ✅ |
| gsoc-2018.json | **399.8** | 1,755.1 | **4.4×** ✅ |

> **Status (Phase 73):** Parse 4/4 ✅ · Serialize 4/4 ✅ — **8/8 complete sweep**. Phase 73 `dump(string&)` eliminated malloc+free per serialize call; citm flipped from **71% behind** yyjson to **2.3× ahead**.

---

## Competitive Comparison

Results from `benchmarks/RESULTS.md` (2026-02-05), Linux x86_64, Release build.

### twitter.json (617 KB)

| Library | Parse (μs) | Serialize (μs) | Parse rank |
|:---|---:|---:|:---:|
| **beast::lazy** | **189** | **145** | 1st |
| yyjson | 282 | 131 | 2nd |
| simdjson | ~570 | — | 3rd |
| RapidJSON | ~1,200 | ~400 | 4th |
| nlohmann | 4,352 | 1,932 | 5th |

### canada.json (2.2 MB, dense floats)

| Library | Parse (μs) | Serialize (μs) |
|:---|---:|---:|
| **beast::lazy** | **1,433** | **789** |
| yyjson | 2,595 | 3,301 |
| beast::rtsm (legacy) | 1,869 | — |
| nlohmann | 23,386 | 9,296 |

> Beast's AVX-512 64B whitespace skip delivers the largest canada.json gain — the file has long runs of `  ` whitespace between floating-point coordinates.

### Simple struct (Person × 10,000)

| Library | Parse (μs/iter) | Serialize (μs/iter) |
|:---|---:|---:|
| yyjson | **0.11** | 0.15 |
| **beast::lazy** | 0.28 | **0.13** |
| nlohmann | 0.92 | 0.38 |

> Beast serializes structs faster than yyjson (pre-flagged separators); parse is slightly slower due to tape overhead on tiny documents.

### Glaze (C++23 reflection)

| Library | Parse (μs) | Serialize (μs) |
|:---|---:|---:|
| Glaze (C++23) | **1.23** | **0.65** |
| **beast::lazy** (C++17/20) | 2.17 | 1.91 |

> Glaze achieves ~40% better throughput using C++23 compile-time reflection — no tape overhead. Beast's C++17 compatibility and zero-copy DOM give it architectural advantages at larger document sizes.

---

## Benchmark Files — Characteristics

| File | Size | Structure | Bottleneck |
|:---|---:|:---|:---|
| **twitter.json** | 617 KB | Social graph, mixed types, many short strings, nested objects | Key scanning (KeyLenCache caches 2,187 scans after first object) |
| **canada.json** | 2.2 MB | Dense floating-point coordinate arrays, minimal nesting | `parse_number` fractional-part serial dependency chain (unavoidable 45+ cycle chain) |
| **citm_catalog.json** | 1.7 MB | Event catalog, 243 repeated-schema `performance` objects, 9 keys each | Repeated key scanning (KeyLenCache converts to O(1) byte compare after first object) |
| **gsoc-2018.json** | 3.2 MB | Large array of similar objects, long string values | String copy (NEON store path on AArch64 amortizes per-char overhead) |

---

## How to Reproduce

### 1. Build benchmarks

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAST_JSON_BUILD_BENCHMARKS=ON \
    "-DCMAKE_CXX_FLAGS=-march=native"
cmake --build build -j$(nproc)
```

### 2. Obtain benchmark files

```bash
# Download standard JSON benchmark suite
curl -L https://raw.githubusercontent.com/nicowillis/benchmarks/master/twitter.json \
    -o twitter.json
# canada.json, citm_catalog.json, gsoc-2018.json from:
# https://github.com/miloyip/nativejson-benchmark/tree/master/data
```

### 3. Run

```bash
cd build
./benchmarks/bench_all --all      # all 4 files, parse + serialize, 300 iterations
./benchmarks/bench_file_io        # file I/O including disk read
./benchmarks/bench_lazy_dom       # DOM accessor microbenchmarks
./benchmarks/bench_ser_profile    # serialize-only profiling target
```

### 4. PGO build for best parse performance

```bash
# Step 1: instrument build
cmake -S . -B build-pgo-gen \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAST_JSON_BUILD_BENCHMARKS=ON \
    "-DCMAKE_CXX_FLAGS=-fprofile-generate -march=native"
cmake --build build-pgo-gen

# Step 2: run to collect profiles (parse workload only)
./build-pgo-gen/benchmarks/bench_all --parse-only

# Step 3: optimized build
cmake -S . -B build-pgo-use \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAST_JSON_BUILD_BENCHMARKS=ON \
    "-DCMAKE_CXX_FLAGS=-fprofile-use -fprofile-correction -march=native"
cmake --build build-pgo-use

# Step 4: measure
./build-pgo-use/benchmarks/bench_all --all
```

**Parse-only PGO profile (Phase 75A):** Using a parse-only profile (not mixed parse+serialize) is critical. Mixed profiling causes LTO to interleave serialize code into parse hot paths, degrading parse performance. On citm: parse-only PGO restored **698 → 626 μs** (−10.3%).

### 5. Apple M1 LLVM PGO

```bash
# Requires Apple Clang 17+
cmake -S . -B build-pgo-gen \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAST_JSON_BUILD_BENCHMARKS=ON \
    "-DCMAKE_CXX_FLAGS=-fprofile-generate=./pgo_profiles -march=native"
cmake --build build-pgo-gen
./build-pgo-gen/benchmarks/bench_all --all

llvm-profdata merge pgo_profiles/*.profraw -o pgo_profiles/merged.profdata

cmake -S . -B build-pgo-use \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAST_JSON_BUILD_BENCHMARKS=ON \
    "-DCMAKE_CXX_FLAGS=-fprofile-instr-use=./pgo_profiles/merged.profdata -march=native"
cmake --build build-pgo-use
./build-pgo-use/benchmarks/bench_all --all
```

---

## Optimization Phase Summary

Key phases that delivered the largest gains, in chronological order:

| Phase | Platform | Change | Effect |
|:---|:---|:---|:---:|
| Phase 44–48 | x86_64 | Action LUT, AVX-512 WS skip, SWAR-8 pre-gate, PGO, prefetch | Foundation |
| Phase 50+53 | x86_64 | Stage 1+2 two-phase AVX-512 parsing + remove `,`/`:` from positions | twitter −44.7% |
| Phase 57 | AArch64 | Pure NEON (remove all scalar gates) | twitter 260→245 μs |
| Phase 58-A | AArch64 | Prefetch distance 192B→256B | canada −3% |
| Phase 59+65 | x86_64 | KeyLenCache O(1) key scan + guard simplification | citm −23% → +21% |
| Phase 60-A | AArch64 | Compact `cur_state_` uint8 state machine | canada −15.8% |
| Phase 61+62 | AArch64 | NEON overlapping-pair dump + 32B inline scan | dump −5.5% |
| Phase 64 | x86_64 | LUT-based `push()` sep+state (2 table loads vs 14 instructions) | gsoc −8% |
| Phase 72-M1 | M1 | NEON 9–16B single-store for StringRaw in serialize | serialize +30–300% |
| Phase 73 | All | `dump(string&)` buffer-reuse overload (eliminate malloc per call) | citm Snappy −71% |
| Phase 75A | x86_64 | Parse-only PGO profile split | citm 698→626 μs |
| Phase 75B | x86_64 | `last_dump_size_` cache (zero-fill eliminated) | citm serialize −22.4% |
| Phase 75-M1 | M1 | NEON slen 1–16 full expansion | citm serialize 233→174 μs |
| Phase 79-M1 | M1 | Branchless sep dispatch (table lookup) | citm +8%, canada +7% |
| Phase 80-M1 | M1 | StringRaw slen≤16 branch reorder (I-cache coherence) | citm 174→166 μs (tied) |

---

## Understanding the Numbers

### Why Beast is fast

1. **8-byte TapeNode** — 33% smaller working set than 12-byte predecessor; fits more tokens per cache line.
2. **Two-phase AVX-512 parsing** — Stage 1 builds positions array at 64 B/iter; Stage 2 does O(1) string length via pre-recorded quote positions.
3. **Pre-flagged separators** — separator (`sep` field) computed at parse time, eliminating the state machine from `dump()` hot loop.
4. **SWAR string scanning** — 8 bytes/cycle GPR scan without any SIMD; ~36% of strings exit in the first 8-byte chunk.
5. **KeyLenCache** — converts repeated-schema key scanning from O(key_length) SIMD to O(1) single byte compare.
6. **Buffer reuse** — `dump(string&)` + `last_dump_size_` cache eliminates malloc and zero-fill on repeated serialize.
7. **Register-resident tape cursor** — `tape_head_` local variable keeps the write pointer in a CPU register across the entire `parse()` body.

### Why some numbers are surprising

**canada.json parse is bottlenecked by hardware, not software.**
The fractional part of a floating-point number requires a serial multiply-add chain: `d = d*10 + digit`. Each step depends on the previous — minimum ~45 CPU cycles of latency, zero parallelism. This is true for yyjson, simdjson, and Beast alike. Beast wins on canada because of better whitespace handling (AVX-512 64B skip), not better number parsing.

**M1 parse lags despite M1's speed.**
Apple M1's 576-entry reorder buffer is extraordinarily wide, enabling yyjson's flat `uint64_t[]` iterator to run many iterations in flight simultaneously. Beast's tape-indirect access pattern (`tape[i].offset` → source pointer) introduces a pointer chain that saturates M1's load-store bandwidth before the ROB depth helps. This is a fundamental architectural trade-off.

**Snapdragon serialize is 4–6× faster than yyjson.**
yyjson on Snapdragon allocates a fresh `malloc` buffer per `dump()` call. Android's scudo allocator makes this expensive for large files (1.7–3.2 MB). Beast's `dump(string&)` reuses the buffer — the per-call overhead drops from O(malloc + zero-fill + free) to O(0) after the first call.

---

## Performance Tuning Knobs

### 1. Use `dump(string&)` instead of `dump()`

```cpp
std::string buf;
for (auto& json : stream) {
    root = beast::parse(doc, json);
    root.dump(buf);   // reuses buf — no malloc after first call
}
```

### 2. Reuse `Document` across parses

```cpp
beast::Document doc;
for (auto& json : stream) {
    auto root = beast::parse(doc, json);  // tape reset, no malloc if capacity sufficient
    process(root);
}
```

### 3. Use `std::string_view` for zero-copy string access

```cpp
// Zero-copy: returns a view into the source buffer
std::string_view sv = root["name"].as<std::string_view>();
// vs allocating:
std::string s = root["name"].as<std::string>();  // heap allocation
```

### 4. Use parse-only PGO

Mixing parse and serialize in the PGO training run degrades parse performance by ~10% on citm due to LTO layout interference. Profile parse and serialize separately if both matter.

### 5. Avoid `operator[]` chains in hot loops

Each `operator[]` on an object performs a linear tape scan. For repeated access to the same key, extract once:

```cpp
// Slower (3 linear scans per iteration)
for (int i : range)
    process(root["a"], root["b"], root["c"]);

// Faster (3 scans total)
auto a = root["a"], b = root["b"], c = root["c"];
for (int i : range)
    process(a, b, c);
```

### 6. Enable AVX-512

Beast's largest parse gains on x86_64 come from AVX-512 (64B whitespace skip, Stage 1 structural indexing). Verify your CPU and compiler flags:

```bash
# Check AVX-512 availability
grep -m1 avx512 /proc/cpuinfo

# Enable in CMake
cmake ... "-DCMAKE_CXX_FLAGS=-march=native"
# or explicitly:
cmake ... "-DCMAKE_CXX_FLAGS=-mavx512f -mavx512bw -mavx512vl"
```

---

## Known Limits

### M1 Pro parse — architectural ceiling

M1 parse for 3/4 files trails yyjson by 5–21%:

| File | Beast (μs) | yyjson (μs) | Gap |
|:---|---:|---:|:---:|
| twitter.json | 205 | 170 | −21% |
| canada.json | 1,501 | 1,432 | −5% |
| citm_catalog.json | 534 | 462 | −16% |

Root cause: M1's 576-entry ROB enables yyjson's flat-array iterator to sustain more in-flight iterations than Beast's tape-indirect access. No algorithmic optimization can overcome this without changing the tape architecture itself.

**Conclusion:** Achieving M1 parse parity with yyjson requires a non-tape parser architecture (e.g., GJSON-style or simdjson-style DOM-less design). This remains open research.

### citm serialize on x86_64

Beast serialize trails yyjson by 33% on citm (312 μs vs 235 μs). The gap was 49% before Phase 73+75B. Root cause: citm has many short strings (1–8 chars); yyjson's serialize loop is better optimized for short strings on x86_64. Further improvement requires restructuring the serialize hot loop for short-string dominance — complex LTO interaction makes this risky.

### SVE on Android (Snapdragon)

SVE/SVE2 is present in Snapdragon 8 Gen 2 hardware but disabled by the Android kernel at EL1. Using SVE intrinsics will trigger `SIGILL`. Future work: measure SVE on Graviton 3 (AWS, Oracle Cloud Always Free Ampere A1) where SVE is kernel-enabled.

### Maximum file size

`TapeNode::offset` is `uint32_t` (4 bytes), limiting source JSON to **4 GB** maximum. `TapeNode::length` is `uint16_t` (16 bits), limiting individual string/number tokens to **65,535 bytes**. Both are sufficient for all practical JSON documents.
