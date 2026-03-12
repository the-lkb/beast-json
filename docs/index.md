---
layout: home

hero:
  name: "Beast JSON"
  text: "C++20 JSON parsing and serialization"
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
  - title: Speed
    details: AVX-512 and NEON SIMD structural scanning. Up to 2.7 GB/s parsing, 8.1 GB/s serialization.
  - title: Zero-Allocation
    details: Flat tape DOM — no tree nodes, no pointer chasing, string views into the input buffer.
  - title: Modern C++20
    details: Concepts-based API. No legacy SFINAE, no macros beyond the optional struct mapping helper.
  - title: Nexus Fusion
    details: Skip the tape entirely. FNV-1a compile-time dispatch maps JSON keys directly to struct fields.
  - title: Hardened
    details: ASan, UBSan, and TSan clean. RFC 6901 JSON Pointer and RFC 6902 JSON Patch with transactional rollback.
---
