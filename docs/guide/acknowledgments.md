# Acknowledgments & References

qbuem-json is built on the shoulders of giants. Our performance and correctness are
the result of integrating decades of research in parsing, numerical algorithms, string
processing, and systems engineering.

We are committed to **transparent attribution** — every algorithm borrowed or adapted
from an external source is documented here, in the source code, and in the relevant
theory pages.

---

## 🔢 Numeric Serialization Algorithms

### Raffaello Giulietti — Schubfach Algorithm

- **What we use:** The core double-to-decimal algorithm that produces the **shortest
  round-trip decimal string** for any IEEE 754 `double`. Used in `qj_nc::to_chars(double)`.
- **Why it matters:** Schubfach guarantees that parsing the output gives back the exact
  same `double`. No fallback path, no precision loss, no trailing-zero bloat.
- **Source:** *"The Schubfach way to render doubles,"* Raffaello Giulietti, 2020.
  [PDF (Google Docs)](https://drive.google.com/file/d/1IEeATSVnEE6TkrHlCYNY2GjaraBjOT4f)

### yyjson — ibireme / Y. Yuan (MIT License)

- **What we use:** Both the **Schubfach port** and the **yy-itoa integer serialization
  algorithm** are adapted from [yyjson](https://github.com/ibireme/yyjson), an MIT-licensed
  single-file JSON library by Y. Yuan.

  | Adapted component | Origin in yyjson |
  |:---|:---|
  | `qj_nc::to_chars(uint32/int32/uint64/int64)` | `yy_write_u32` / `yy_write_u64` integer routines |
  | `qj_nc::to_chars(double)` | `yyjson_write_double` + `f64_bin_to_dec` (Schubfach) |
  | 128-bit pow10 table (1,336 entries) | `f64_pow10_sig_table` in `yyjson.c` |
  | 2-digit ASCII table `char_table[200]` | `yy_digit_table` in `yyjson.c` |

- **License:** MIT. qbuem-json preserves this in both source comments and here.
- **Repository:** [https://github.com/ibireme/yyjson](https://github.com/ibireme/yyjson)

> **Why yyjson's implementation?**
> yyjson is itself one of the fastest JSON libraries in existence. Their numeric
> serialization routines are carefully hand-optimized and well-tested against all
> IEEE 754 corner cases. Rather than writing a parallel implementation from scratch,
> we port and integrate their work directly, with full credit.

---

## 🔬 Mathematical Foundations

### Russ Cox (Google)

- **Fast Unrounded Scaling**: The cornerstone of our bit-accurate **number parsing**.
  His research simplified what was historically a complex and error-prone part of
  systems programming. qbuem-json uses this for `parse()` stage-2 number reading.
- [Floating-Point Printing and Parsing Can Be Simple and Fast (2026)](https://research.swtch.com/fp-all/)

### Daniel Lemire & Michael Eisel

- **Eisel-Lemire Algorithm**: For the high-speed 64-bit parsing path that handles
  the vast majority of well-formed JSON numbers without any bignum fallback.
- [fast_float: 28 GB/s floating-point parsing](https://github.com/fastfloat/fast_float)

---

## 🏎️ SIMD & Architectural Inspiration

### simdjson Project

- **Geoff Langdale & Daniel Lemire**: For the groundbreaking work on **two-phase SIMD
  parsing** and structural indexing which heavily influenced our Tape DOM design.
  The two-stage pipeline (Stage 1: SIMD structural scan → Stage 2: scalar tape
  generation) is a direct extension of this architecture.
- [simdjson: Parsing gigabytes of JSON per second](https://github.com/simdjson/simdjson)
- *Langdale & Lemire, "Parsing Gigabytes of JSON per Second," VLDB Journal, 2020.*
  [DOI](https://doi.org/10.1007/s00778-019-00578-5)

### yyjson (Tape DOM Architecture)

In addition to the numeric algorithms above, yyjson independently developed a
**flat Tape DOM** architecture (similar to simdjson's) that demonstrated the
performance ceiling achievable with a contiguous node array. This validated our
own DOM Tape design and provided a direct competitive reference point.

### glaze — Stephen Berry et al.

- **What we drew from:** The [glaze](https://github.com/stephenberry/glaze) library's
  approach to **compile-time struct registration** and **FNV-1a hash dispatch** for
  field mapping informed the design of qbuem-json's Nexus Fusion engine.
  Specifically:
  - Compile-time `consteval` FNV-1a hashing for zero-overhead field lookup
  - The `if constexpr` dispatch chain used in `append_json<W, T>` for type-generic serialization
  - The FastWriter concept (pre-grown buffer with raw-pointer writes) was refined
    in response to profiling against glaze's serialization benchmark results
- **License:** MIT.
- [https://github.com/stephenberry/glaze](https://github.com/stephenberry/glaze)

> **Transparency note:** qbuem-json benchmarks include glaze as a primary competitor.
> We believe open acknowledgment of mutual influence strengthens the ecosystem.
> Where qbuem-json Nexus wins, it is because we extended these ideas further.
> Where glaze wins (or ties), those results are published honestly in our CI benchmarks.

### RapidJSON

- **Milo Yip**: For creating the "gold standard" of C++ JSON libraries for a decade
  and providing the modular Handler-based design inspiration.
- [RapidJSON Documentation](https://rapidjson.org/)

---

## 🛠️ Infrastructure & Tools

- **VitePress**: For the beautiful documentation framework.
- **Google Benchmark (Google Test + Google Mock)**: For the test infrastructure that
  validates every claim.
- **GitHub Actions**: For CI across three native hardware runners (x86\_64, aarch64, Apple Silicon).

---

## 📝 Full Reference List

1. **Raffaello Giulietti**, *"The Schubfach way to render doubles,"* 2020.
   [PDF](https://drive.google.com/file/d/1IEeATSVnEE6TkrHlCYNY2GjaraBjOT4f)

2. **Y. Yuan (ibireme)**, *yyjson — A fast JSON library in ANSI C,* 2020–present.
   [GitHub (MIT)](https://github.com/ibireme/yyjson)

3. **Russ Cox**, *"Floating-Point Printing and Parsing Can Be Simple and Fast,"* 2026.
   [Blog](https://research.swtch.com/fp-all/)

4. **Daniel Lemire**, *"Number Parsing at Gigabytes per Second,"* SPE, 2021.
   [arXiv](https://arxiv.org/abs/2101.11408)

5. **Michael Eisel & Daniel Lemire**, *"The Eisel-Lemire Algorithm,"* 2020.
   [fast_float](https://github.com/fastfloat/fast_float)

6. **Langdale & Lemire**, *"Parsing Gigabytes of JSON per Second,"* VLDB Journal, 2020.
   [DOI](https://doi.org/10.1007/s00778-019-00578-5)

7. **Stephen Berry et al.**, *glaze — Extremely fast, in-memory, JSON and interface library,* 2022–present.
   [GitHub (MIT)](https://github.com/stephenberry/glaze)

---

::: tip ❤️ To the C++ Community
To the C++ community: thank you for pushing the boundaries of what is possible with
modern C++. Every open-source library listed here made qbuem-json possible.
We hope to contribute back in kind.
:::
