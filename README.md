<div align="center">

  <!-- CI & Security -->
  <p>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml/badge.svg" alt="Sanitizers (ASan · UBSan · TSan)"></a>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml/badge.svg" alt="Benchmark CI"></a>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml/badge.svg" alt="CodeQL"></a>
  </p>

  <!-- Compliance & Standards -->
  <p>
    <a href="https://en.cppreference.com/w/cpp/20"><img src="https://img.shields.io/badge/C%2B%2B-20-blue" alt="C++20"></a>
    <a href="https://qbuem.com/qbuem-json/guide/correctness#rfc-8259-compliance"><img src="https://img.shields.io/badge/RFC%208259-compliant-brightgreen" alt="RFC 8259 compliant"></a>
    <a href="https://qbuem.com/qbuem-json/guide/correctness#rfc-8259-compliance"><img src="https://img.shields.io/badge/RFC%206901-JSON%20Pointer-brightgreen" alt="RFC 6901 JSON Pointer"></a>
    <a href="https://qbuem.com/qbuem-json/guide/correctness#rfc-8259-compliance"><img src="https://img.shields.io/badge/RFC%206902-JSON%20Patch-brightgreen" alt="RFC 6902 JSON Patch"></a>
    <a href="https://qbuem.com/qbuem-json/guide/correctness#ieee-754-floating-point-correctness"><img src="https://img.shields.io/badge/IEEE%20754-round--trip-brightgreen" alt="IEEE 754 round-trip"></a>
  </p>

  <!-- Testing -->
  <p>
    <a href="https://qbuem.com/qbuem-json/guide/correctness"><img src="https://img.shields.io/badge/tests-521%20passing-brightgreen" alt="521 tests passing"></a>
    <a href="https://qbuem.com/qbuem-json/guide/correctness#fuzz-testing"><img src="https://img.shields.io/badge/fuzz-3%20libFuzzer%20targets-orange" alt="3 libFuzzer targets"></a>
  </p>

  <!-- Distribution -->
  <p>
    <a href="https://github.com/qbuem/qbuem-json/releases/tag/v1.0.7"><img src="https://img.shields.io/badge/version-v1.0.7-blue" alt="v1.0.7"></a>
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/license-Apache%202.0-blue" alt="Apache 2.0"></a>
    <a href="https://github.com/qbuem/qbuem-json/blob/main/include/qbuem_json/qbuem_json.hpp"><img src="https://img.shields.io/badge/header--only-single%20file-lightgrey" alt="header-only"></a>
    <img src="https://img.shields.io/badge/dependencies-zero-brightgreen" alt="zero dependencies">
    <img src="https://img.shields.io/badge/platforms-Linux%20%7C%20macOS-lightgrey" alt="Linux | macOS">
  </p>

  <p>
    <a href="https://qbuem.com/qbuem-json/">
      <img src="https://img.shields.io/badge/Documentation-Hub-orange?style=for-the-badge&logo=vitepress" alt="Documentation Hub">
    </a>
  </p>

</div>

---

**qbuem-json** is a high-performance C++20 JSON engine providing a **Hybrid Strategy** for modern workloads. It offers two specialized engines: **qbuem-json DOM** for massive-scale throughput and **qbuem-json Nexus** for micro-latency Zero-Tape mapping.

By leveraging **C++20 Concepts**, **SIMD (AVX-512, NEON)**, and **Nexus Fusion (Zero-Tape)** technology, qbuem-json eliminates traditional tree-based DOM overhead while retaining a beautiful, type-safe API.

---

## 🚀 Features

* **Dual-Engine Architecture** — Choose between **qbuem-json DOM** for bulk processing and **qbuem-json Nexus** for sub-microsecond struct mapping.
* **World-Class Performance** — Outperforms `yyjson`, `simdjson`, and `glaze` in real-world complex STL benchmarks.
* **Nexus Fusion (Zero-Tape)** — Direct JSON-to-struct mapping in a single pass. Zero Tape allocations.
* **Zero-Allocation Execution** — Sequential memory layout and zero-copy strings for deterministic performance.
* **Single Header** — Drop `qbuem_json.hpp` into your project and you're ready.
* **Three-stage float parsing** — Eisel-Lemire (~98.8 %) → Russ Cox Unrounded Scaling (~1.2 %) → `std::strtod` (subnormals only). `parse(serialize(x)) == x` for all finite doubles.

```cpp
struct User {
    uint64_t id;
    std::string username;
    std::vector<std::string> tags;
    bool active;
};

QBUEM_JSON_FIELDS(User, id, username, tags, active)
```

---

## ⚡ Performance

**[→ Live CI Benchmark Results](https://qbuem.com/qbuem-json/guide/benchmarks)**

Benchmarks run automatically on every push to `main` across three native GitHub-hosted runners.
Results cover all standard datasets (`twitter`, `canada`, `citm`, `gsoc`, `harsh`) with **Parse**, **Serialize**, and **Memory (Alloc KB)** metrics.

| Runner | Architecture | Compiler |
|:---|:---|:---|
| `ubuntu-latest` | x86_64 | GCC 13, Release |
| `ubuntu-24.04-arm` | Linux aarch64 | GCC 14, Release |
| `macos-latest` | Apple Silicon | Apple Clang, Release |

---

## 📖 Documentation

**[qbuem.com/qbuem-json](https://qbuem.com/qbuem-json/)**

| Category | Topics |
|:---|:---|
| **Engineering Theory** | [Tape DOM](https://qbuem.com/qbuem-json/theory/architecture), [Nexus Fusion](https://qbuem.com/qbuem-json/theory/nexus-fusion), [SIMD Acceleration](https://qbuem.com/qbuem-json/theory/simd), [Russ Cox Algorithm](https://qbuem.com/qbuem-json/theory/russ-cox) |
| **Advanced Usage** | [HFT Optimization Patterns](https://qbuem.com/qbuem-json/guide/hft-patterns), [Custom Allocators](https://qbuem.com/qbuem-json/guide/allocators), [Language Bindings](https://qbuem.com/qbuem-json/guide/bindings) |
| **Guides** | [Getting Started](https://qbuem.com/qbuem-json/guide/getting-started), [Object Mapping](https://qbuem.com/qbuem-json/guide/mapping), [Error Handling](https://qbuem.com/qbuem-json/guide/errors) |
| **API Reference** | [Full C++ Doxygen Reference](https://qbuem.com/qbuem-json/api/reference/index.html) |

---

## 🤝 Open Source Commitment

qbuem-json is licensed under the **Apache License 2.0** — permissive commercial use, modification, and distribution without copyleft friction.

* **Transparent Benchmarking** — All benchmark suites and data files are open and independently verifiable.
* **Community-Driven** — Contributions, critiques, and ideas are welcome.

---

## 💡 Inspiration & Acknowledgements

* **[Raffaello Giulietti](https://drive.google.com/file/d/1IEeATSVnEE6TkrHlCYNY2GjaraBjOT4f)** — Schubfach algorithm (2020). **Implemented** in `qj_nc` namespace (ported from yyjson, MIT). Powers all `double` → shortest-decimal serialization.
* **[yyjson / ibireme (Y. Yuan)](https://github.com/ibireme/yyjson)** — MIT-licensed source of the Schubfach port (`qj_dtoa`) and yy-itoa integer serialization (`qj_itoa`) used in qbuem-json's `qj_nc` namespace.
* **[Michael Eisel & Daniel Lemire](https://arxiv.org/abs/2101.11408)** — Eisel-Lemire algorithm (2020). **Implemented** as stage-1 float parser (`eisel_lemire_f64`). Handles ~98.8 % of decimal → double conversions in constant time via 128-bit multiplication against a pre-built power-of-10 table.
* **[Russ Cox](https://research.swtch.com/fp)** — Fast Unrounded Scaling algorithm (2026). **Implemented** as stage-2 float parser (`russ_cox_uscale_f64`). Resolves the ~1.2 % of inputs Eisel-Lemire cannot, by using the ceiling of the table's high word (`ph_ceil = ph + (pl≠0)`), making the sticky bit always decisive. Proved correct by the [Ivy companion proof](https://research.swtch.com/fp-proof).
* **[simdjson](https://github.com/simdjson/simdjson)** — Pioneered SIMD-accelerated JSON parsing at L1-cache speeds.
* **[glaze / Stephen Berry](https://github.com/stephenberry/glaze)** — Compile-time FNV-1a dispatch and struct reflection patterns that influenced Nexus Fusion.
* **[RapidJSON](https://github.com/Tencent/rapidjson)** — A decade of excellence in C++ JSON processing.

## 📝 References

1. **Raffaello Giulietti**, *"The Schubfach way to render doubles,"* 2020. [PDF](https://drive.google.com/file/d/1IEeATSVnEE6TkrHlCYNY2GjaraBjOT4f)
2. **Y. Yuan (ibireme)**, *yyjson — A fast JSON library in ANSI C,* MIT License. [GitHub](https://github.com/ibireme/yyjson)
3. **Michael Eisel & Daniel Lemire**, *"Number Parsing at a Gigabyte per Second,"* Software: Practice and Experience, 2021. [arXiv:2101.11408](https://arxiv.org/abs/2101.11408)
4. **Russ Cox**, *"Floating-Point Printing and Parsing Can Be Simple and Fast,"* 2026. [research.swtch.com/fp](https://research.swtch.com/fp) · Companion proof: [research.swtch.com/fp-proof](https://research.swtch.com/fp-proof)
5. **Langdale & Lemire**, *"Parsing Gigabytes of JSON per Second,"* VLDB Journal, 2020. [DOI](https://doi.org/10.1007/s00778-019-00578-5)
6. **Stephen Berry et al.**, *glaze — Extremely fast, in-memory, JSON and interface library,* MIT License. [GitHub](https://github.com/stephenberry/glaze)

---

## ⚖️ License

qbuem-json is licensed under the **Apache License 2.0**.

* **Open Attribution** — The `qj_nc` numeric serialization namespace is adapted from
  [yyjson](https://github.com/ibireme/yyjson) (MIT License, Y. Yuan) with full attribution
  in source and documentation. All other code is an original C++20 implementation.
* **Fair Use & Citation** — Academic and blog citations provided for attribution under fair use.
