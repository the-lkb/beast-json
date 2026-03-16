---
layout: home

hero:
  name: "qbuem-json"
  text: "Feel the Power of Ultimate JSON Speed"
  tagline: "small changes, big future · C++20 · Single Header · Zero Dependencies · AVX-512 & NEON SIMD"
  actions:
    - theme: brand
      text: Get Started
      link: /guide/getting-started
    - theme: alt
      text: Benchmarks
      link: /guide/benchmarks
    - theme: alt
      text: GitHub
      link: https://github.com/qbuem/qbuem-json

features:
  - icon: ⚡
    title: "Up to 2.5 GB/s Parsing"
    details: "AVX-512 (x86) and ARM NEON SIMD structural scanning. Two-pass tape architecture eliminates all per-node allocation overhead. Numbers from live CI — see Benchmarks page."

  - icon: 🎯
    title: "Zero-Allocation Design"
    details: "Flat tape DOM — no tree nodes, no pointer chasing. String views point directly into the input buffer. One contiguous array."

  - icon: 🔷
    title: "Modern C++20 API"
    details: "Concepts-based interface. No legacy SFINAE. Optional struct mapping via a single QBUEM_JSON_FIELDS macro."

  - icon: 🚀
    title: "Nexus Fusion Engine"
    details: "Skip the tape entirely. FNV-1a compile-time dispatch maps JSON keys directly to struct fields — zero intermediate allocations."

  - icon: 🔒
    title: "RFC Compliant & Hardened"
    details: "RFC 6901 JSON Pointer. RFC 6902 JSON Patch with transactional rollback. 521 tests · ASan, UBSan, TSan run on every commit · 3 libFuzzer targets."

  - icon: 📦
    title: "Single Header · Apache 2.0"
    details: "Drop in qbuem_json.hpp and ship. No build system changes. No transitive dependencies. Free for commercial use."
---


<div style="max-width: 900px; margin: 4rem auto; padding: 0 1.5rem;">

<div style="margin: 2rem 0 1.75rem;">

  <!-- Row 1: CI Status -->
  <div style="display: flex; align-items: center; gap: 0.4rem; flex-wrap: wrap; margin-bottom: 0.5rem;">
    <span style="font-size: 0.68rem; font-weight: 700; letter-spacing: 0.07em; text-transform: uppercase; color: #999; min-width: 5.5rem; flex-shrink: 0;">CI Status</span>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml/badge.svg" alt="CI" /></a>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml/badge.svg" alt="Sanitizers (ASan · UBSan · TSan)" /></a>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml/badge.svg" alt="Benchmark CI" /></a>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml/badge.svg" alt="CodeQL" /></a>
  </div>

  <!-- Row 2: Standards -->
  <div style="display: flex; align-items: center; gap: 0.4rem; flex-wrap: wrap; margin-bottom: 0.5rem;">
    <span style="font-size: 0.68rem; font-weight: 700; letter-spacing: 0.07em; text-transform: uppercase; color: #999; min-width: 5.5rem; flex-shrink: 0;">Standards</span>
    <a href="https://en.cppreference.com/w/cpp/20"><img src="https://img.shields.io/badge/C%2B%2B-20-blue" alt="C++20" /></a>
    <a href="/guide/correctness#rfc-8259-compliance"><img src="https://img.shields.io/badge/RFC%208259-compliant-brightgreen" alt="RFC 8259" /></a>
    <a href="/guide/correctness#rfc-8259-compliance"><img src="https://img.shields.io/badge/RFC%206901-JSON%20Pointer-brightgreen" alt="RFC 6901" /></a>
    <a href="/guide/correctness#rfc-8259-compliance"><img src="https://img.shields.io/badge/RFC%206902-JSON%20Patch-brightgreen" alt="RFC 6902" /></a>
    <a href="/guide/correctness#ieee-754-floating-point-correctness"><img src="https://img.shields.io/badge/IEEE%20754-round--trip-brightgreen" alt="IEEE 754 round-trip" /></a>
  </div>

  <!-- Row 3: Testing -->
  <div style="display: flex; align-items: center; gap: 0.4rem; flex-wrap: wrap; margin-bottom: 0.5rem;">
    <span style="font-size: 0.68rem; font-weight: 700; letter-spacing: 0.07em; text-transform: uppercase; color: #999; min-width: 5.5rem; flex-shrink: 0;">Testing</span>
    <a href="/guide/correctness"><img src="https://img.shields.io/badge/tests-521%20passing-brightgreen" alt="521 tests passing" /></a>
    <a href="/guide/correctness#fuzz-testing"><img src="https://img.shields.io/badge/fuzz-3%20libFuzzer%20targets-orange" alt="3 libFuzzer targets" /></a>
  </div>

  <!-- Row 4: Package -->
  <div style="display: flex; align-items: center; gap: 0.4rem; flex-wrap: wrap;">
    <span style="font-size: 0.68rem; font-weight: 700; letter-spacing: 0.07em; text-transform: uppercase; color: #999; min-width: 5.5rem; flex-shrink: 0;">Package</span>
    <a href="https://github.com/qbuem/qbuem-json/releases/tag/v1.0.7"><img src="https://img.shields.io/badge/version-v1.0.7-blue" alt="v1.0.7" /></a>
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/license-Apache%202.0-blue" alt="Apache 2.0" /></a>
    <a href="https://github.com/qbuem/qbuem-json/blob/main/include/qbuem_json/qbuem_json.hpp"><img src="https://img.shields.io/badge/header--only-single%20file-lightgrey" alt="header-only" /></a>
    <img src="https://img.shields.io/badge/dependencies-zero-brightgreen" alt="zero dependencies" />
    <img src="https://img.shields.io/badge/platforms-Linux%20%7C%20macOS-lightgrey" alt="Linux | macOS" />
  </div>

</div>

## Why qbuem-json?

**qbuem-json** was built for production systems where latency and allocation count — HFT tick data, real-time game state, large-scale data pipelines. Every design decision is measurable: benchmarks run on CI across three architectures, 521 automated tests guard correctness, and the library ships as a single header with zero dependencies.

<div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 1.25rem; margin: 2rem 0;">

<div style="background: #f5f0e8; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.25rem;">
  <div style="font-size: 1.5rem; margin-bottom: 0.5rem;">📊</div>
  <div style="font-weight: 800; color: #1e2e5c; font-size: 1.1rem; margin-bottom: 0.35rem;">Parsing throughput</div>
  <div style="color: rgba(30,46,92,0.65); font-size: 0.88rem; line-height: 1.55;">
    twitter.json (617 KB) · from <a href="/guide/benchmarks">live CI results</a>:<br>
    x86_64 GCC 13: <strong>2.9 GB/s</strong><br>
    Linux aarch64 GCC 14: <strong>2.4 GB/s</strong><br>
    Apple Silicon: <strong>2.5 GB/s</strong>
  </div>
</div>

<div style="background: #f5f0e8; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.25rem;">
  <div style="font-size: 1.5rem; margin-bottom: 0.5rem;">🏎️</div>
  <div style="font-weight: 800; color: #1e2e5c; font-size: 1.1rem; margin-bottom: 0.35rem;">Serialization throughput</div>
  <div style="color: rgba(30,46,92,0.65); font-size: 0.88rem; line-height: 1.55;">
    twitter.json (617 KB) · from <a href="/guide/benchmarks">live CI results</a>:<br>
    x86_64 GCC 13: <strong>5.3 GB/s</strong><br>
    Linux aarch64 GCC 14: <strong>6.1 GB/s</strong><br>
    Apple Silicon: <strong>7.2 GB/s</strong>
  </div>
</div>

<div style="background: #f5f0e8; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.25rem;">
  <div style="font-size: 1.5rem; margin-bottom: 0.5rem;">🧵</div>
  <div style="font-weight: 800; color: #1e2e5c; font-size: 1.1rem; margin-bottom: 0.35rem;">Zero-copy strings</div>
  <div style="color: rgba(30,46,92,0.65); font-size: 0.88rem; line-height: 1.55;"><code>string_view</code> fields point into the original input buffer. No heap allocations during access.</div>
</div>

<div style="background: #f5f0e8; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.25rem;">
  <div style="font-size: 1.5rem; margin-bottom: 0.5rem;">🔬</div>
  <div style="font-weight: 800; color: #1e2e5c; font-size: 1.1rem; margin-bottom: 0.35rem;">IEEE 754 round-trip</div>
  <div style="color: rgba(30,46,92,0.65); font-size: 0.88rem; line-height: 1.55;">
    Three-stage parsing pipeline:<br>
    ① <strong>Eisel-Lemire</strong> (~98.8 %) — 128-bit table, two-multiply<br>
    ② <strong>Russ Cox Unrounded Scaling</strong> (~1.2 %) — ceiling table, always decisive<br>
    ③ <strong><code>strtod</code></strong> — subnormals only (&lt;0.01 %)<br>
    Serialization: <strong>Schubfach</strong> (Giulietti 2020) — shortest decimal, no trailing zeros.<br>
    <code>parse(serialize(x)) == x</code> for all finite doubles. <a href="/guide/correctness#ieee-754-floating-point-correctness">Details →</a>
  </div>
</div>

</div>

## Quick Start

```cpp
#include <qbuem_json/qbuem_json.hpp>

// ── DOM engine — flexible parsing ────────────────────────────────────────
qbuem::Document doc;
qbuem::Value root = qbuem::parse(doc, R"({"user":"Alice","score":42})");

std::string_view name  = root["user"].as<std::string_view>();  // zero-copy
int              score = root["score"].as<int>();

// ── Nexus engine — direct struct mapping ─────────────────────────────────
struct Player {
    std::string name;
    int         score = 0;
};
QBUEM_JSON_FIELDS(Player, name, score)

auto player = qbuem::read<Player>(R"({"name":"Bob","score":99})");
std::string json = qbuem::write(player);
```

## Dual-Engine Architecture

qbuem-json ships **two complementary engines** in a single header:

| Engine | Use Case | Allocation | Throughput (twitter.json) |
|---|---|---|---|
| **DOM (Tape)** | Exploratory access, partial reads, dynamic keys | Single arena | [2.0–2.5 GB/s](/guide/benchmarks) |
| **Nexus Fusion** | Struct mapping, HFT, embedded | Zero tape | Compile-time dispatch |

Choose the DOM engine when key names are dynamic or unknown at compile time. Switch to Nexus when you know your schema upfront and want maximum throughput with deterministic latency.

## Platform Support

| Platform | Compiler | SIMD |
|---|---|---|
| Linux x86_64 | GCC 13+, Clang 18+ | AVX-512 / SSE4.2 |
| Linux aarch64 | GCC 14+, Clang 18+ | ARM NEON |
| macOS Apple Silicon | Apple Clang | ARM NEON |

## Install

**CMake (FetchContent)**
```cmake
include(FetchContent)
FetchContent_Declare(qbuem_json
    GIT_REPOSITORY https://github.com/qbuem/qbuem-json.git
    GIT_TAG        v1.0.7
)
FetchContent_MakeAvailable(qbuem_json)
target_link_libraries(my_target PRIVATE qbuem_json::qbuem_json)
```

**Single Header (manual)**
```bash
# Copy include/qbuem_json/qbuem_json.hpp into your project
cp include/qbuem_json/qbuem_json.hpp /path/to/your/project/
```

**Compile flags**
```bash
g++ -std=c++20 -O3 -march=native my_app.cpp -o my_app
```

## Correctness & Safety

We are a new library and we take verification seriously.  Every claim here links
to CI you can inspect:

<div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 1rem; margin: 1.5rem 0 2rem;">

<div style="background: #f0f4ff; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.1rem;">
  <div style="font-weight: 700; color: #1e2e5c; margin-bottom: 0.4rem;">521 tests · 20 files</div>
  <div style="color: rgba(30,46,92,0.7); font-size: 0.86rem; line-height: 1.55;">5,556 lines of C++ tests covering DOM, Nexus, STL mapping, error handling, Unicode, and edge cases.  <a href="/guide/correctness">Details →</a></div>
</div>

<div style="background: #f0f4ff; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.1rem;">
  <div style="font-weight: 700; color: #1e2e5c; margin-bottom: 0.4rem;">73 RFC 8259 tests</div>
  <div style="color: rgba(30,46,92,0.7); font-size: 0.86rem; line-height: 1.55;">y_/n_/i_ JSONTestSuite naming convention.  RFC 6901 JSON Pointer and RFC 6902 JSON Patch with transactional rollback.  <a href="/guide/correctness#rfc-8259-compliance">Details →</a></div>
</div>

<div style="background: #f0f4ff; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.1rem;">
  <div style="font-weight: 700; color: #1e2e5c; margin-bottom: 0.4rem;">ASan · UBSan · TSan</div>
  <div style="color: rgba(30,46,92,0.7); font-size: 0.86rem; line-height: 1.55;">Sanitizer jobs run on every commit via <a href="https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml">sanitizers.yml</a>.  Heap overflow, use-after-free, data races — all caught automatically.</div>
</div>

<div style="background: #f0f4ff; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.1rem;">
  <div style="font-weight: 700; color: #1e2e5c; margin-bottom: 0.4rem;">3 libFuzzer targets</div>
  <div style="color: rgba(30,46,92,0.7); font-size: 0.86rem; line-height: 1.55;">fuzz_dom · fuzz_parse · fuzz_rfc8259.  Seed corpus from real-world benchmark datasets.  <a href="/guide/correctness#fuzz-testing">Details →</a></div>
</div>

<div style="background: #f0f4ff; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.1rem;">
  <div style="font-weight: 700; color: #1e2e5c; margin-bottom: 0.4rem;">10-config CI matrix</div>
  <div style="color: rgba(30,46,92,0.7); font-size: 0.86rem; line-height: 1.55;">GCC 13/14 · Clang 18 · Apple Clang · x86_64 · aarch64 · Apple Silicon · Debug + Release — native runners, no QEMU. <a href="https://github.com/qbuem/qbuem-json/blob/main/.github/workflows/ci.yml">ci.yml →</a></div>
</div>

<div style="background: #f0f4ff; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.1rem;">
  <div style="font-weight: 700; color: #1e2e5c; margin-bottom: 0.4rem;">IEEE 754 round-trip</div>
  <div style="color: rgba(30,46,92,0.7); font-size: 0.86rem; line-height: 1.55;"><code>parse(serialize(x)) == x</code> for all finite doubles. Parsing: Eisel-Lemire (~98.8 %) → Russ Cox Unrounded Scaling (~1.2 %) → <code>strtod</code> (subnormals). Serialization: Schubfach (Giulietti 2020).  <a href="/guide/correctness#ieee-754-floating-point-correctness">Details →</a></div>
</div>

</div>

<div style="text-align: center; margin: 0 0 3rem;">
  <a href="/guide/correctness" style="display: inline-block; background: #1e2e5c; color: white; padding: 0.65rem 1.6rem; border-radius: 8px; font-weight: 600; text-decoration: none; font-size: 0.95rem;">View full correctness report →</a>
</div>

</div>
