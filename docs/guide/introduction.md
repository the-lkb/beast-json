# Introduction

Beast JSON is a C++20 JSON library built around two parsing engines: a tape-based DOM engine and a zero-tape struct mapping engine (Nexus). Both ship in a single header with no dependencies.

---

## Two engines, one header

**DOM engine (`beast::parse`)** builds a flat tape of nodes in a single pass. No pointer chasing, no tree allocation — the output is a contiguous array you can traverse or mutate. SIMD structural scanning (AVX-512 on x86, NEON on ARM) gets through the input at 64 bytes per cycle before the parser starts.

**Nexus engine (`beast::fuse<T>`)** skips the tape entirely. It maps JSON keys directly to struct fields using compile-time FNV-1a hashes, writing each value into the struct as it's encountered. No intermediate representation, no second pass.

```cpp
beast::Document doc;
auto root = beast::parse(doc, json);    // DOM: tape → Value tree

User u;
beast::fuse(u, json);                   // Nexus: stream → struct, no tape
```

The two engines handle different shapes of work. DOM is the right choice when the schema isn't known at compile time, or when you need to inspect, mutate, or partially traverse arbitrary JSON. Nexus is right when you have a fixed struct and want it filled as fast as possible.

---

## What's in the library

- AVX-512 and ARM NEON structural indexing
- Schubfach dtoa for shortest round-trip float serialization
- yy-itoa for integer serialization without division
- Russ Cox algorithm for decimal-to-double parsing
- JSON Pointer (RFC 6901) and JSON Patch (RFC 6902)
- Language bindings for Python and Go

[Getting Started](/guide/getting-started) has the install step and first example.
