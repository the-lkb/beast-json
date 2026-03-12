<div align="center">

  <p>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml/badge.svg" alt="C++20 CI"></a>
    <a href="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml/badge.svg" alt="CodeQL Static Analysis"></a>
    <a href="https://github.com/qbuem/qbuem-json/releases"><img src="https://img.shields.io/badge/Version-v1.0.5-blue" alt="Version 1.0.5"></a>
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache%202.0-blue.svg" alt="License: Apache 2.0"></a>
  </p>

  <p>
    <a href="https://qbuem.github.io/qbuem-json/">
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

**[→ Live CI Benchmark Results](https://qbuem.github.io/qbuem-json/guide/benchmarks)**

Benchmarks run automatically on every push to `main` across three native GitHub-hosted runners.
Results cover all standard datasets (`twitter`, `canada`, `citm`, `gsoc`, `harsh`) with **Parse**, **Serialize**, and **Memory (Alloc KB)** metrics.

| Runner | Architecture | Compiler |
|:---|:---|:---|
| `ubuntu-latest` | x86_64 | GCC 13, Release |
| `ubuntu-24.04-arm` | Linux aarch64 | GCC 14, Release |
| `macos-latest` | Apple Silicon | Apple Clang, Release |

---

## 📖 Documentation

**[qbuem.github.io/qbuem-json](https://qbuem.github.io/qbuem-json/)**

| Category | Topics |
|:---|:---|
| **Engineering Theory** | [Tape DOM](https://qbuem.github.io/qbuem-json/theory/architecture), [Nexus Fusion](https://qbuem.github.io/qbuem-json/theory/nexus-fusion), [SIMD Acceleration](https://qbuem.github.io/qbuem-json/theory/simd), [Russ Cox Algorithm](https://qbuem.github.io/qbuem-json/theory/russ-cox) |
| **Advanced Usage** | [HFT Optimization Patterns](https://qbuem.github.io/qbuem-json/guide/hft-patterns), [Custom Allocators](https://qbuem.github.io/qbuem-json/guide/allocators), [Language Bindings](https://qbuem.github.io/qbuem-json/guide/bindings) |
| **Guides** | [Getting Started](https://qbuem.github.io/qbuem-json/guide/getting-started), [Object Mapping](https://qbuem.github.io/qbuem-json/guide/mapping), [Error Handling](https://qbuem.github.io/qbuem-json/guide/errors) |
| **API Reference** | [Full C++ Doxygen Reference](https://qbuem.github.io/qbuem-json/api/reference/index.html) |

---

## 🤝 Open Source Commitment

qbuem-json is licensed under the **Apache License 2.0** — permissive commercial use, modification, and distribution without copyleft friction.

* **Transparent Benchmarking** — All benchmark suites and data files are open and independently verifiable.
* **Community-Driven** — Contributions, critiques, and ideas are welcome.

---

## 💡 Inspiration & Acknowledgements

* **[Raffaello Giulietti](https://drive.google.com/file/d/1IEeATSVnEE6TkrHlCYNY2GjaraBjOT4f)** — Schubfach algorithm (2020), foundation for shortest round-trip double serialization.
* **[yyjson / ibireme (Y. Yuan)](https://github.com/ibireme/yyjson)** — MIT-licensed source of the Schubfach port and yy-itoa integer serialization used in qbuem-json's `qj_nc` namespace.
* **[Russ Cox](https://research.swtch.com/fp-all/)** — Fast Unrounded Scaling algorithm (2026), foundation for bit-accurate number **parsing**.
* **[Daniel Lemire](https://github.com/lemire) & Michael Eisel** — Eisel-Lemire algorithm and [`fast_float`](https://github.com/fastfloat/fast_float), defining modern 64-bit float parsing.
* **[simdjson](https://github.com/simdjson/simdjson)** — Pioneered SIMD-accelerated JSON parsing at L1-cache speeds.
* **[glaze / Stephen Berry](https://github.com/stephenberry/glaze)** — Compile-time FNV-1a dispatch and struct reflection patterns that influenced Nexus Fusion.
* **[RapidJSON](https://github.com/Tencent/rapidjson)** — A decade of excellence in C++ JSON processing.

## 📝 References

1. **Raffaello Giulietti**, *"The Schubfach way to render doubles,"* 2020. [PDF](https://drive.google.com/file/d/1IEeATSVnEE6TkrHlCYNY2GjaraBjOT4f)
2. **Y. Yuan (ibireme)**, *yyjson — A fast JSON library in ANSI C,* MIT License. [GitHub](https://github.com/ibireme/yyjson)
3. **Russ Cox**, *"Floating-Point Printing and Parsing Can Be Simple and Fast,"* 2026. [Blog](https://research.swtch.com/fp-all/)
4. **Daniel Lemire**, *"Number Parsing at Gigabytes per Second,"* SPE, 2021. [arXiv](https://arxiv.org/abs/2101.11408)
5. **Michael Eisel & Daniel Lemire**, *"The Eisel-Lemire Algorithm,"* 2020. [fast_float](https://github.com/fastfloat/fast_float)
6. **Langdale & Lemire**, *"Parsing Gigabytes of JSON per Second,"* VLDB Journal, 2020. [DOI](https://doi.org/10.1007/s00778-019-00578-5)
7. **Stephen Berry et al.**, *glaze — Extremely fast, in-memory, JSON and interface library,* MIT License. [GitHub](https://github.com/stephenberry/glaze)

---

## ⚖️ License

qbuem-json is licensed under the **Apache License 2.0**.

* **Open Attribution** — The `qj_nc` numeric serialization namespace is adapted from
  [yyjson](https://github.com/ibireme/yyjson) (MIT License, Y. Yuan) with full attribution
  in source and documentation. All other code is an original C++20 implementation.
* **Fair Use & Citation** — Academic and blog citations provided for attribution under fair use.
