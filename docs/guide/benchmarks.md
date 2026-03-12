# Performance Benchmarks

qbuem-json is engineered to be the benchmark leader in both parsing and serialization across all major architectures.

Automatically updated on every push to `main` that touches `include/` or `benchmarks/`.
Runs in parallel on three native GitHub-hosted runners (x86\_64 / Apple Silicon / Linux AArch64).

| Runner | Architecture | Compiler |
|:---|:---|:---|
| `ubuntu-latest` | x86_64 | GCC 13, Release |
| `ubuntu-24.04-arm` | Linux aarch64 | GCC 14, Release |
| `macos-latest` | Apple Silicon | Apple Clang, Release |

<BenchmarkCi />

---

## 🔗 Object Mapping (Nexus Fusion)

Unlike DOM-based parsing, **Nexus Fusion** maps JSON directly to C++ structs. These benchmarks measure the latency of full deserialization into complex STL types.

> [!TIP]
> Use the "Dataset" selector above to switch between Standard DOM (twitter.json, etc.) and Object Mapping (Simple Object, Extreme Harsh STL) results.

---

## ⚙️ Architecture-Specific Optimizations

| Architecture | Technique | Benefit |
|:---|:---|:---|
| **x86_64** | AVX-512 64B/iter Stage 1 scan | Extracts structural tokens in a single pass, ahead of parsing |
| **x86_64** | BMI2 `PEXT`/`PDEP` | Bit-manipulation for separator extraction without branches |
| **Apple Silicon** | NEON 16B vectorized bitmasks | Branchless character detection tuned for 128-byte M-series cache lines |
| **All** | SWAR (SIMD Within A Register) | 8 bytes/cycle string scan using 64-bit GPR — no SIMD required for short strings |
| **All** | KeyLenCache | O(1) key lookup for repeated object schemas (e.g., JSON arrays of same-shape objects) |
| **All** | Schubfach dtoa (R. Giulietti / yyjson port) | Shortest round-trip double→decimal — 2–3× faster than `std::to_chars(double)`, no trailing zeros |
| **All** | yy-itoa (Y. Yuan / yyjson port) | Integer→decimal via 2-digit table + multiply-shift — zero division instructions |
| **All** | Russ Cox algorithm (parsing only) | Shortest round-trip decimal parsing — no `strtod`, no bignum fallback |
