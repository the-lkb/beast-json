---
layout: home

hero:
  name: "Beast JSON"
  text: "The Ultimate High-Performance C++20 JSON Engine"
  tagline: "Single header. Zero dependencies. AVX-512 accelerated. Zero-allocation design."
  image:
    src: /logo.png
    alt: Beast JSON Logo
  actions:
    - theme: brand
      text: Get Started
      link: /guide/getting-started
    - theme: alt
      text: View Benchmarks
      link: /guide/benchmarks

features:
  - title: 🚀 World-Class Speed
    details: Leverages AVX-512 and NEON SIMD to achieve up to 2.7 GB/s parsing and 8.1 GB/s serialization throughput.
  - title: 💾 Zero-Allocation
    details: Uses a sequential Array-Backed Tape DOM and memory-mapped string views for true zero-cost processing.
  - title: 🛠️ Modern C++20
    details: Built with C++20 Concepts and Ranges. Clean, expressive API without the legacy boilerplate.
  - title: 🔗 Nexus Fusion
    details: Zero-Tape mapping engine. Directly fuses JSON streams to C++ structs with perfect-hash O(1) dispatch.
  - title: 🔒 Hardened & Secure
    details: 100% memory safe. Verified with ASan, UBSan, TSan, and extensive security stress tests.
---

  <h2>The Hybrid Strategy</h2>
  <p style="max-width: 800px; margin: 0 auto; line-height: 1.6; font-size: 1.1rem;">
    Beast JSON is the only C++ engine providing a **Dual-Engine** strategy. 
    Choose **Beast (DOM)** for massive throughput using SIMD Stage 1 structural scanning, 
    or switch to **Beast (Nexus)** for sub-microsecond latency using our Zero-Tape 
    direct mapping technology. Overcome the "one size fits all" throughput barrier.
  </p>

<div style="display: flex; justify-content: space-around; flex-wrap: wrap; gap: 2rem; padding: 2rem;">
  <div style="flex: 1; min-width: 300px;">
    <h3>Vision</h3>
    <p>To provide a zero-cost abstraction for JSON that feels built-in to the C++ type system, without sacrificing a single CPU cycle.</p>
  </div>
  <div style="flex: 1; min-width: 300px;">
    <h3>Direction</h3>
    <p>Continuous optimization for next-gen hardware, deepening SIMD utilization (AVX-512/SVE), and expanding language binding ecosystem.</p>
  </div>
</div>
