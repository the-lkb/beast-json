---
layout: home

hero:
  name: "qbuem-json"
  text: "Feel the Power of Ultimate JSON Speed"
  tagline: "small changes, big future · C++20 · Single Header · Zero Dependencies · AVX-512 & NEON SIMD"
  image:
    src: /logo.svg
    alt: qbuem-json Logo
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
    title: "2.7 GB/s Parsing"
    details: "AVX-512 (x86) and ARM NEON SIMD structural scanning. Two-pass tape architecture eliminates all per-node allocation overhead."

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
    details: "RFC 6901 JSON Pointer. RFC 6902 JSON Patch with transactional rollback. ASan, UBSan, TSan clean on every commit."

  - icon: 📦
    title: "Single Header · Apache 2.0"
    details: "Drop in qbuem_json.hpp and ship. No build system changes. No transitive dependencies. Free for commercial use."
---

<div style="background: #f5f0e5; padding: 2rem 1.5rem; margin-top: 0; text-align: center; border-top: 1px solid rgba(26,39,68,0.10); border-bottom: 1px solid rgba(26,39,68,0.10);">
  <img src="/banner.svg" alt="qbuem-json — Feel the Power of Ultimate JSON Speed"
       style="max-width: 900px; width: 100%; margin: 0 auto; border-radius: 10px; border: 1px solid rgba(26,39,68,0.10); box-shadow: 0 8px 32px rgba(26,39,68,0.12);" />
</div>

<div style="max-width: 900px; margin: 4rem auto; padding: 0 1.5rem;">

## Why qbuem-json?

Engineers at every level of the stack need JSON that doesn't get in the way. **qbuem-json** was designed by a team with deep roots in high-frequency trading infrastructure, game engine development, and large-scale data pipelines. Every design decision traces back to a real production constraint.

<div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 1.25rem; margin: 2rem 0;">

<div style="background: #f5f0e8; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.25rem;">
  <div style="font-size: 1.5rem; margin-bottom: 0.5rem;">📊</div>
  <div style="font-weight: 800; color: #1e2e5c; font-size: 1.1rem; margin-bottom: 0.35rem;">2.7 GB/s parse</div>
  <div style="color: rgba(30,46,92,0.65); font-size: 0.88rem; line-height: 1.55;">Measured on twitter.json (631 KB) with GCC 13 -O3 -march=native on x86_64.</div>
</div>

<div style="background: #f5f0e8; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.25rem;">
  <div style="font-size: 1.5rem; margin-bottom: 0.5rem;">🏎️</div>
  <div style="font-weight: 800; color: #1e2e5c; font-size: 1.1rem; margin-bottom: 0.35rem;">8.1 GB/s serialize</div>
  <div style="color: rgba(30,46,92,0.65); font-size: 0.88rem; line-height: 1.55;">Schubfach shortest round-trip floats + yy-itoa integer serialization without division.</div>
</div>

<div style="background: #f5f0e8; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.25rem;">
  <div style="font-size: 1.5rem; margin-bottom: 0.5rem;">🧵</div>
  <div style="font-weight: 800; color: #1e2e5c; font-size: 1.1rem; margin-bottom: 0.35rem;">Zero-copy strings</div>
  <div style="color: rgba(30,46,92,0.65); font-size: 0.88rem; line-height: 1.55;"><code>string_view</code> fields point into the original input buffer. No heap allocations during access.</div>
</div>

<div style="background: #f5f0e8; border: 1px solid rgba(30,46,92,0.15); border-radius: 10px; padding: 1.25rem;">
  <div style="font-size: 1.5rem; margin-bottom: 0.5rem;">🔬</div>
  <div style="font-weight: 800; color: #1e2e5c; font-size: 1.1rem; margin-bottom: 0.35rem;">Eisel-Lemire floats</div>
  <div style="color: rgba(30,46,92,0.65); font-size: 0.88rem; line-height: 1.55;">Fast decimal-to-double with Russ Cox 2026 fallback. Correct for all 64-bit IEEE 754 values.</div>
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
    QBUEM_JSON_FIELDS(name, score)
};

auto player = qbuem::read<Player>(R"({"name":"Bob","score":99})");
std::string json = qbuem::write(player);
```

## Dual-Engine Architecture

qbuem-json ships **two complementary engines** in a single header:

| Engine | Use Case | Allocation | Throughput |
|---|---|---|---|
| **DOM (Tape)** | Exploratory access, partial reads, dynamic keys | Single arena | ~2.7 GB/s |
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
    GIT_TAG        v1.0.5
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

</div>
