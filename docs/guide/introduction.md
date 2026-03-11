# Introduction

Beast JSON is the flagship JSON engine for modern C++. It is engineered to deliver the absolute peak performance possible on today's superscalar CPUs.

## 🏆 The Beast Identity

"Beast" reflects the uncompromising nature of this library's performance. In an industry where "fast enough" is often the status quo, Beast JSON targets **hardware-limit performance.**

### Design Priorities:
1. **Speed First**: Outperform all competitors in real-world workloads.
2. **Zero Overhead**: Eliminate pointer chasing and small allocations (the **Lazy Tape DOM** model).
3. **C++20 Native**: Clean code over legacy SFINAE madness.
4. **Safety**: 100% sanitizer-clean code with transactional safety.

## 🚀 The Dual-Engine Strategy

Beast JSON is unique in providing a **Hybrid Strategy**. We don't force a single "best" parser; instead, we offer two specialized engines to handle different data scales:

- **Beast (DOM)**: The **High-Throughput Engine**. Optimized for SIMD Stage 1 structural scanning. It excels at massive datasets, skipping whitespace at 64B/cycle.
- **Beast (Nexus)**: The **Low-Latency Engine**. Optimized for zero-intermediate mapping. By bypassing the Tape/DOM entirely, it delivers the lowest possible latency for structured C++ objects.

By leveraging the latest C++20 features (Concepts, Ranges, `<charconv>`) and multi-architecture SIMD, we've created a tool that allows you to express your logic clearly while the engine handles the extreme optimization under the hood.

## 🗺️ Scope of this Documentation

This technical hub is designed for:
- **Architects**: Looking to understand the Tape DOM and SIMD dispatching internals.
- **Developers**: Needing a clean, automated mapping for their C++ types.
- **HFT Engineers**: Seeking deterministic, zero-allocation runtime behavior.
- **System Integrators**: Looking to bind the fastest JSON engine to other languages.

---

Ready to experience the speed? Jump to the [Getting Started](/guide/getting-started) guide.
