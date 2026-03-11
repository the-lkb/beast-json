# Performance Benchmarks

Beast JSON is engineered to be the benchmark leader in both parsing and serialization across all major architectures.

Automatically updated on every push to `main` that touches `include/` or `benchmarks/`.
Runs in parallel on three native GitHub-hosted runners (x86\_64 / Apple Silicon / Linux AArch64).

| Runner | Architecture | Compiler |
|:---|:---|:---|
| `ubuntu-latest` | x86_64 | GCC 13, Release |
| `ubuntu-24.04-arm` | Linux aarch64 | GCC 14, Release |
| `macos-latest` | Apple Silicon | Apple Clang, Release |

<BenchmarkCi />

---

## ⚙️ Architecture-Specific Optimizations

| Architecture | Technique | Benefit |
|:---|:---|:---|
| **x86_64** | AVX-512 64B/iter Stage 1 scan | Extracts structural tokens in a single pass, ahead of parsing |
| **x86_64** | BMI2 `PEXT`/`PDEP` | Bit-manipulation for separator extraction without branches |
| **Apple Silicon** | NEON 16B vectorized bitmasks | Branchless character detection tuned for 128-byte M-series cache lines |
| **All** | SWAR (SIMD Within A Register) | 8 bytes/cycle string scan using 64-bit GPR — no SIMD required for short strings |
| **All** | KeyLenCache | O(1) key lookup for repeated object schemas (e.g., JSON arrays of same-shape objects) |
| **All** | Russ Cox float printer | Shortest round-trip decimal — 20-30% faster than `printf("%.17g")` |
