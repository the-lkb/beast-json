# Performance Benchmarks

Beast JSON is engineered to be the benchmark leader in both parsing and serialization across all major architectures.

> All timings measured using **`-O3 -march=native` / `-mcpu=apple-m1` + LTO + PGO** on dedicated GitHub Actions runners. Format: **Parse μs / Serialize μs**. **Bold** = fastest in category.

---

## 🏎 Parsing & Serialization — Standard Datasets

Four standard JSON datasets: `twitter.json` (631 KB), `canada.json` (2.2 MB), `citm_catalog.json` (1.7 MB), `gsoc-2018.json` (3.1 MB).

### 🖥 Intel x86_64 (GCC 13.3, AVX-512, PGO)

| Library | twitter (Parse/Ser) | canada (Parse/Ser) | citm (Parse/Ser) | gsoc (Parse/Ser) |
|:---|---:|---:|---:|---:|
| **Beast JSON** | 265 μs / **149 μs** | **1876 μs** / **952 μs** | **597 μs** / 326 μs | **531 μs** / **273 μs** |
| `simdjson` | **242 μs** / 815 μs | 2509 μs / 10163 μs | 732 μs / 1101 μs | 945 μs / 5568 μs |
| `yyjson` | 749 μs / 153 μs | 2987 μs / 3968 μs | 2102 μs / **251 μs** | 2020 μs / 1083 μs |
| `RapidJSON` | 1294 μs / 826 μs | 5386 μs / 7475 μs | 2196 μs / 830 μs | 7085 μs / 4593 μs |
| `Glaze` | 2251 μs / 363 μs | 8297 μs / 4130 μs | 3697 μs / 958 μs | 4226 μs / 1245 μs |
| `nlohmann` | 6142 μs / 2211 μs | 37308 μs / 10197 μs | 11852 μs / 2681 μs | 25726 μs / 15255 μs |

### 🖥 Apple M-Series (Apple Clang, NEON, PGO)

| Library | twitter (Parse/Ser) | canada (Parse/Ser) | citm (Parse/Ser) | gsoc (Parse/Ser) |
|:---|---:|---:|---:|---:|
| **Beast JSON** | 229 μs / **75 μs** | 1925 μs / **878 μs** | 563 μs / 199 μs | **752 μs** / **293 μs** |
| `simdjson` | 228 μs / 439 μs | 2466 μs / 7943 μs | **501 μs** / 646 μs | 1206 μs / 6041 μs |
| `yyjson` | **187 μs** / 108 μs | **1868 μs** / 2563 μs | 644 μs / **177 μs** | 1208 μs / 1050 μs |
| `RapidJSON` | 926 μs / 998 μs | 2888 μs / 7330 μs | 1404 μs / 937 μs | 6733 μs / 7011 μs |
| `Glaze` | 1698 μs / 239 μs | 7073 μs / 2826 μs | 2789 μs / 710 μs | 2755 μs / 836 μs |
| `nlohmann` | 3745 μs / 1449 μs | 19787 μs / 7451 μs | 9749 μs / 2125 μs | 14657 μs / 13456 μs |

### 🖥 Linux AArch64 (GCC/Clang, NEON)

| Library | twitter (Parse/Ser) | canada (Parse/Ser) | citm (Parse/Ser) | gsoc (Parse/Ser) |
|:---|---:|---:|---:|---:|
| **Beast JSON** | 3254 μs / **1164 μs** | **19708 μs** / **8229 μs** | 9464 μs / 2822 μs | **7429 μs** / **2031 μs** |
| `simdjson` | 7620 μs / 4158 μs | 32192 μs / 41013 μs | 16566 μs / 6913 μs | 30977 μs / 22403 μs |
| `yyjson` | **2874 μs** / 1503 μs | 21540 μs / 25113 μs | **7919 μs** / **2678 μs** | 10255 μs / 6799 μs |
| `RapidJSON` | 6652 μs / 3239 μs | 32500 μs / 46085 μs | 13451 μs / 4967 μs | 25648 μs / 15419 μs |
| `Glaze` | 20924 μs / 2081 μs | 85146 μs / 19867 μs | 36008 μs / 5719 μs | 32766 μs / 5502 μs |
| `nlohmann` | 56953 μs / 13042 μs | 372197 μs / 52692 μs | 95103 μs / 17262 μs | 249566 μs / 67940 μs |

---

## 🌪 Extreme Heavy-Load Benchmarks

Testing with a **5.5 MB stress file**: 50,000 deeply nested objects, arrays, floats, and heavily escaped strings (`\n`, `\t`, `\r`, escaped quotes). This exercises the parser's absolute worst-case fallback path.

### 🖥 Intel x86_64

| Library | Parse Time | Serialize Time |
|:---|---:|---:|
| **Beast JSON** | 7.71 ms | **3.71 ms** |
| `simdjson` | **6.04 ms** | 16.34 ms |
| `yyjson` | 9.13 ms | 4.49 ms |
| `RapidJSON` | 17.55 ms | 14.14 ms |
| `Glaze` | 90.33 ms | 39.23 ms |
| `nlohmann` | 164.70 ms | 35.95 ms |

### 🖥 Apple M-Series

| Library | Parse Time | Serialize Time |
|:---|---:|---:|
| **Beast JSON** | 5.28 ms | **2.32 ms** |
| `simdjson` | **4.89 ms** | 12.16 ms |
| `yyjson` | 5.29 ms | 3.53 ms |
| `RapidJSON` | 13.15 ms | 11.90 ms |
| `Glaze` | 35.96 ms | 10.69 ms |
| `nlohmann` | 58.68 ms | 17.44 ms |

### 🖥 Linux AArch64

| Library | Parse Time | Serialize Time |
|:---|---:|---:|
| **Beast JSON** | 81.11 ms | **32.32 ms** |
| `simdjson` | 104.28 ms | 119.38 ms |
| `yyjson` | **78.12 ms** | 49.15 ms |
| `RapidJSON` | 109.74 ms | 79.94 ms |
| `Glaze` | 457.51 ms | 88.45 ms |
| `nlohmann` | 1034.05 ms | 190.70 ms |

---

## 🪶 Memory Efficiency

Peak Resident Set Size (RSS) while parsing `twitter.json` (631.5 KB), measured via macOS `mach_task`. Beast JSON achieves the lowest memory footprint in the industry by using a minimalist 8-byte `TapeNode` and true zero-copy strings.

| Library | Peak RSS | DOM Memory | Overhead Ratio |
|:---|---:|---:|---:|
| **Beast JSON** | **3.44 MB** | **0.23 MB** | **0.36×** |
| `Glaze` | 5.58 MB | 1.80 MB | 2.85× |
| `yyjson` | 6.32 MB | 2.50 MB | 3.96× |
| `simdjson` | 11.04 MB | 6.50 MB | 10.29× |

> Beast JSON's **Tape DOM** model eliminates pointer overhead and node fragmentation — giving it the lowest per-document memory cost in the C++ ecosystem.

---

## ⚙️ Architecture-Specific Optimizations

| Architecture | Technique | Benefit |
|:---|:---|:---|
| **x86_64** | AVX-512 64B/iter Stage 1 scan | Extracts structural tokens in a single pass, ahead of parsing |
| **x86_64** | BMI2 `PEXT`/`PDEP` | Bit-manipulation for separator extraction without branches |
| **Apple Silicon** | NEON 16B vectorized bitmasks | Branchless character detection tuned for 128-byte M-series cache lines |
| **All** | SWAR (SIMD Within A Register) | 8 bytes/cycle string scan using 64-bit GPR — no SIMD required for short strings |
| **All** | KeyLenCache | O(1) key lookup for repeated object schemas (e.g., JSON arrays of same-shape objects) |
| **All** | Russ Cox float printer | Shortest round-trip decimal — faster than `printf("%.17g")` by 20-30% |
