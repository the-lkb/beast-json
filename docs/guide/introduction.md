# Introduction

Beast JSON is the flagship JSON engine for modern C++. It is engineered to deliver the absolute peak performance possible on today's superscalar CPUs.

## 🏆 The Beast Identity

"Beast" reflects the uncompromising nature of this library's performance. In an industry where "fast enough" is often the status quo, Beast JSON targets **hardware-limit performance.**

### Design Priorities:
1. **Speed First**: Outperform all competitors in real-world workloads.
2. **Zero Overhead**: Eliminate pointer chasing and small allocations (the **Lazy Tape DOM** model).
3. **C++20 Native**: Clean code over legacy SFINAE madness.
4. **Safety**: 100% sanitizer-clean code with transactional safety.

## 🚀 Why another JSON library?

While libraries like `simdjson` are incredibly fast for parsing, and `nlohmann/json` is incredibly easy to use, Beast JSON was born to bridge that gap: 
**A library that is as fast as the fastest, but feels as natural as the easiest.**

By leveraging the latest C++20 features (Concepts, Ranges, `<charconv>`) and multi-architecture SIMD, we've created a tool that allows you to express your logic clearly while the engine handles the extreme optimization under the hood.

## 🗺️ Scope of this Documentation

This technical hub is designed for:
- **Architects**: Looking to understand the Tape DOM and SIMD dispatching internals.
- **Developers**: Needing a clean, automated mapping for their C++ types.
- **HFT Engineers**: Seeking deterministic, zero-allocation runtime behavior.
- **System Integrators**: Looking to bind the fastest JSON engine to other languages.

---

Ready to experience the speed? Jump to the [Getting Started](/guide/getting-started) guide.
