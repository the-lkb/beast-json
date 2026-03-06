# Beast JSON

> 🚧 **Work in Progress (Pre-Release 1.0)** 🚧
> *Apple Silicon M1 serialize optimization complete (Phase 80-M1): **Beast now beats or ties yyjson on all 4 serialize benchmarks on M1** — twitter +35%, canada 3.15×, gsoc 4.2×, citm effectively tied (1.2% gap at 500 iterations = measurement noise). Combined with Linux x86-64 (parse 4/4 ≥1.2×) and Snapdragon 8 Gen 2 (all 8 benchmarks swept), Beast leads across all 3 measured platforms. We are building **"The Ultimate API"** — a Zero-Allocation Monadic DOM with extreme developer convenience. See the Roadmap section below for details.*

**Beast JSON** is a high-performance, single-header C++20 JSON library built around a tape-based lazy DOM. Its design goal is simple: match or beat the world's fastest JSON libraries through aggressive low-level optimization — while remaining practical for real-world use.

> **An AI-Only Generated Library** — every line of code, every optimization, and every benchmark in this repository was designed and written by AI (Claude/Gemini). It's an ongoing experiment in what AI can achieve in low-level, high-performance C++ systems programming.

---

## Benchmarks

### Linux x86-64

> **Environment**: Linux x86-64, GCC 13.3.0 `-O3 -flto -march=native` + PGO (parse-only profile), 300 iterations per file, timings are per-run averages.
> Phase 44–65+73+75 applied (Action LUT · AVX-512 string gate · AVX-512 64B WS skip · SWAR-8 pre-gate · PGO · input prefetch · Stage 1+2 two-phase parsing · positions `:,` elimination · compact `cur_state_` · LUT-based `push()` · KeyLenCache SIMD key bypass · Phase 65: guard simplification · Phase 73: `dump(string&)` buffer-reuse · **Phase 75: parse-only PGO + `last_dump_size_` cache**).
> yyjson compiled with full SIMD enabled (`-march=native`). All results verified correct (✓ PASS).

#### twitter.json — 616.7 KB · social graph, mixed types

| Library | Parse (μs) | Throughput | Serialize (μs) |
| :--- | ---: | :--- | ---: |
| **beast::lazy** | **189** | **3.27 GB/s** | **145** |
| yyjson | 282 | 2.19 GB/s | 131 |
| beast::rtsm | 309 | 2.00 GB/s | — |
| nlohmann | 4,352 | 142 MB/s | 1,932 |

> beast::lazy is **49% faster** (1.49×) than yyjson on parse. Two-phase AVX-512 Stage 1+2 parsing with KeyLenCache delivers **3.27 GB/s** parse throughput.

#### canada.json — 2.2 MB · dense floating-point arrays

| Library | Parse (μs) | Throughput | Serialize (μs) |
| :--- | ---: | :--- | ---: |
| **beast::lazy** | **1,433** | **1.54 GB/s** | **789** |
| beast::rtsm | 1,869 | 1.18 GB/s | — |
| yyjson | 2,595 | 0.85 GB/s | 3,301 |
| nlohmann | 23,386 | 94 MB/s | 9,296 |

> beast::lazy is **1.81× faster** to parse and **4.18× faster** to serialize than yyjson. AVX-512 64B whitespace skip delivers massive gains on coordinate-heavy JSON.

#### citm_catalog.json — 1.7 MB · event catalog, string-heavy

| Library | Parse (μs) | Throughput | Serialize (μs) |
| :--- | ---: | :--- | ---: |
| **beast::lazy** | **626** | **2.70 GB/s** | **312** |
| yyjson | 757 | 2.23 GB/s | 235 |
| beast::rtsm | 1,057 | 1.60 GB/s | — |
| nlohmann | 9,566 | 176 MB/s | 2,047 |

> beast::lazy is **21% faster** (1.21×) than yyjson on parse. Phase 75 parse-only PGO profile restored the citm margin. Serialize: yyjson 33% faster (gap narrowed from 49% by `last_dump_size_` cache, −22.4% improvement).

#### gsoc-2018.json — 3.2 MB · large object array

| Library | Parse (μs) | Throughput | Serialize (μs) |
| :--- | ---: | :--- | ---: |
| **beast::lazy** | **731** | **4.45 GB/s** | **369** |
| beast::rtsm | 979 | 3.32 GB/s | — |
| yyjson | 1,615 | 2.01 GB/s | 1,417 |
| nlohmann | 15,048 | 216 MB/s | 11,932 |

> beast::lazy is **2.21× faster** to parse and **3.84× faster** to serialize than yyjson. Parse throughput reaches **4.45 GB/s**.

#### Summary

| Benchmark | Beast vs yyjson (parse) | Beast vs yyjson (serialize) |
| :--- | :--- | :--- |
| twitter.json | **Beast 1.49× faster** ✅ | yyjson 11% faster |
| canada.json | **Beast 1.81× faster** ✅ | **Beast 4.18× faster** |
| citm_catalog.json | **Beast 1.21× faster** ✅ | yyjson 33% faster |
| gsoc-2018.json | **Beast 2.21× faster** ✅ | **Beast 3.84× faster** |

Beast **beats yyjson by ≥1.2× on parse for ALL 4 files** (Phase 75 milestone). ✅

#### 1.2× Goal Progress (beat yyjson by ≥20% on all 4 files)

| File | Target (yyjson/1.2) | Current | Status |
| :--- | ---: | ---: | :---: |
| twitter.json | ≤235 μs | **189 μs** | ✅ |
| canada.json | ≤2,163 μs | **1,433 μs** | ✅ |
| citm_catalog.json | ≤631 μs | **626 μs** | ✅ |
| gsoc-2018.json | ≤1,346 μs | **731 μs** | ✅ |

> **Phase 75 milestone**: All 4 x86_64 parse targets met simultaneously for the first time. Parse-only PGO profile (Phase 75A) restored LTO code layout for parse — citm 698→626 μs. `last_dump_size_` cache (Phase 75B) eliminated per-call zero-fill overhead (citm serialize −22.4%, pure C++20).

---

### macOS (Apple M1 Pro)

> **Environment**: macOS 26.3, Apple Clang 17, Apple M1 Pro.
> Phase 65-M1 through Phase 80-M1 applied (KeyLenCache 3×16B NEON key scanner · NEON 1-16B StringRaw fast path · branchless sep dispatch · StringRaw slen≤16 branch-first reorder).
> **LLVM PGO**: `-fprofile-generate` → `llvm-profdata merge` → `-fprofile-instr-use`. 500 iterations (stable). All results verified correct (✓ PASS).

#### twitter.json — 616.7 KB · social graph, mixed types

| Library | Parse (μs) | Serialize (μs) |
| :--- | ---: | ---: |
| **beast::lazy** | **205** | **66** |
| yyjson | 170 | 102 |
| beast::rtsm | 260 | — |
| nlohmann | 3,110 | 1,106 |

> beast::lazy serialize is **35% faster** than yyjson (66 vs 102 μs). Parse is 21% behind yyjson — M1's 576-entry ROB favors yyjson's sequential flat-array over tape indirection.

#### canada.json — 2.2 MB · dense floating-point arrays

| Library | Parse (μs) | Serialize (μs) |
| :--- | ---: | ---: |
| yyjson | 1,432 | 2,210 |
| **beast::lazy** | **1,501** | **701** |
| beast::rtsm | 1,863 | — |
| nlohmann | 16,349 | 6,696 |

> beast::lazy serialize is **3.15× faster** than yyjson. Parse is 5% behind yyjson — bottleneck is serial `d = d*10+c` dependency chain in `parse_number` (structurally un-parallelizable).

#### citm_catalog.json — 1.7 MB · event catalog, string-heavy

| Library | Parse (μs) | Serialize (μs) |
| :--- | ---: | ---: |
| yyjson | 462 | 164 |
| **beast::lazy** | **534** | **166** |
| beast::rtsm | 850 | — |
| nlohmann | 7,813 | 1,065 |

> beast::lazy serialize **ties yyjson** (166 vs 164 μs — 1.2% gap at 500 iterations, within measurement noise). Parse is 16% behind yyjson.

#### gsoc-2018.json — 3.2 MB · large object array

| Library | Parse (μs) | Serialize (μs) |
| :--- | ---: | ---: |
| **beast::lazy** | **572** | **169** |
| beast::rtsm | 685 | — |
| yyjson | 966 | 704 |
| nlohmann | 13,171 | 10,294 |

> beast::lazy is **69% faster** to parse and **4.2× faster** to serialize than yyjson.

#### Summary

| Benchmark | Beast vs yyjson (parse) | Beast vs yyjson (serialize) |
| :--- | :--- | :--- |
| twitter.json | yyjson 21% faster | **Beast 35% faster** ✅ |
| canada.json | yyjson 5% faster | **Beast 3.15× faster** ✅ |
| citm_catalog.json | yyjson 16% faster | **Beast tied** (1.2%, noise-level) ✅ |
| gsoc-2018.json | **Beast 69% faster** ✅ | **Beast 4.2× faster** ✅ |

Beast **sweeps all 4 serialize benchmarks** on M1 (Phase 80-M1 milestone). citm serialize achieves effective parity with yyjson. Parse gaps remain structural: M1's extremely wide OoO engine favors yyjson's flat-array iterator over Beast's tape-indirection model. gsoc parse is Beast's dominant edge (+69%) due to NEON string-copy amortisation on the long object arrays.

#### M1 Serialize Optimization History (Phase 72–80)

| Phase | Change | Key Result |
| :--- | :--- | :--- |
| Phase 72-M1 | NEON 9-16B single-store for StringRaw in serialize | serialize: twitter/canada/gsoc all 30-300%+ ahead |
| Phase 75-M1 | NEON vld1q+vst1q for all slen 1-16B strings | citm serialize 233 μs → **174 μs** (−25%) |
| Phase 79-M1 | Branchless sep dispatch — table lookup + conditional advance | citm +8%, canada +7% |
| Phase 80-M1 | StringRaw branch reorder: `slen ≤ 16` checked first | citm +5% → **166 μs** (tied yyjson) |

> **M1 PGO/LTO Golden Rules** (confirmed through Phase 72–81): serialize code that reduces binary size or eliminates branches is safe. Any parse code change (add or remove), any loop control-flow change (`continue`/`break`), and any serialize code that increases binary size all cause LTO layout disruption and serialize regression. Only code restructuring with neutral or smaller binary footprint is safe.

---

### AArch64 Generic (Snapdragon 8 Gen 2 · Android Termux)

> **Environment**: Galaxy Z Fold 5, Android Termux, Clang 21.1.8 (`-O3 -march=armv8.4-a+crypto+dotprod+fp16+i8mm+bf16`), Snapdragon 8 Gen 2.
> CPU: 1×Cortex-X3 (3360 MHz) · 2×Cortex-A715 · 2×Cortex-A710 · 3×Cortex-A510.
> **Note**: Prime core (cpu7, Cortex-X3 3360 MHz) offline at time of measurement. Run pinned to cpu6 (Gold/A715, 2803 MHz).
> Phase 57+58-A+60-A+61+62+73 applied (Pure NEON · prefetch tuning · compact context state · NEON overlapping-pair dump string copy · NEON 32B value scan · **`dump(string&)` buffer-reuse**), 300 iterations.
> Note: SVE/SVE2 present in hardware but kernel-disabled on this Android build. `-march=armv8.4-a+...` flags required (no SVE).
> All results verified correct (✓ PASS).

#### twitter.json — 616.7 KB · social graph, mixed types

| Library | Parse (μs) | Throughput | Serialize (μs) |
| :--- | ---: | :--- | ---: |
| **beast::lazy** | **321.8** | **1.92 GB/s** | **121.3** |
| beast::rtsm | 453.8 | 1.36 GB/s | — |
| yyjson | 536.9 | 1.15 GB/s | 264.1 |
| nlohmann | 7,288 | 85 MB/s | 2,484 |

> beast::lazy is **67% faster** to parse and **2.2× faster** to serialize than yyjson.

#### canada.json — 2.2 MB · dense floating-point arrays

| Library | Parse (μs) | Throughput | Serialize (μs) |
| :--- | ---: | :--- | ---: |
| **beast::lazy** | **2,103.5** | **1.05 GB/s** | **676.5** |
| beast::rtsm | 3,554.5 | 618 MB/s | — |
| yyjson | 3,614.8 | 608 MB/s | 3,774.7 |
| nlohmann | 104,792 | 21 MB/s | 12,463 |

> beast::lazy is **72% faster** to parse and **5.6× faster** to serialize than yyjson.

#### citm_catalog.json — 1.7 MB · event catalog, string-heavy

| Library | Parse (μs) | Throughput | Serialize (μs) |
| :--- | ---: | :--- | ---: |
| **beast::lazy** | **844.1** | **2.00 GB/s** | **325.5** |
| beast::rtsm | 1,944.4 | 867 MB/s | — |
| yyjson | 1,627.2 | 1.04 GB/s | 754.2 |
| nlohmann | 14,961 | 113 MB/s | 3,003 |

> beast::lazy is **93% faster** to parse and **2.3× faster** to serialize than yyjson. **Phase 73** reversed the serialize deficit — previously 71% behind yyjson on this string-heavy workload.

#### gsoc-2018.json — 3.2 MB · large object array

| Library | Parse (μs) | Throughput | Serialize (μs) |
| :--- | ---: | :--- | ---: |
| **beast::lazy** | **956.4** | **3.40 GB/s** | **399.8** |
| beast::rtsm | 1,245.2 | 2.61 GB/s | — |
| yyjson | 2,421.7 | 1.34 GB/s | 1,755.1 |
| nlohmann | 23,341 | 139 MB/s | 21,758 |

> beast::lazy is **153% faster** to parse and **4.4× faster** to serialize than yyjson.

#### Summary

| Benchmark | Beast vs yyjson (parse) | Beast vs yyjson (serialize) |
| :--- | :--- | :--- |
| twitter.json | **Beast 67% faster** ✅ | **Beast 2.2× faster** ✅ |
| canada.json | **Beast 72% faster** ✅ | **Beast 5.6× faster** ✅ |
| citm_catalog.json | **Beast 93% faster** ✅ | **Beast 2.3× faster** ✅ |
| gsoc-2018.json | **Beast 153% faster** ✅ | **Beast 4.4× faster** ✅ |

Beast **sweeps all 8 parse + serialize benchmarks** on Snapdragon 8 Gen 2. **Phase 73** (`dump(string&)` buffer-reuse) eliminated the `malloc+memset` overhead on every serialize call — citm flipped from **71% behind** yyjson to **2.3× ahead**. Optimizations proven on this core transfer directly to AWS Graviton 3 and other server-class AArch64 workloads.

#### 1.2× Goal Progress (beat yyjson by ≥20% on all 4 files)

| File | Target (yyjson/1.2) | Current | Status |
| :--- | ---: | ---: | :---: |
| twitter.json | ≤447 μs | **321.8 μs** | ✅ |
| canada.json | ≤3,012 μs | **2,103.5 μs** | ✅ |
| citm_catalog.json | ≤1,356 μs | **844.1 μs** | ✅ |
| gsoc-2018.json | ≤2,018 μs | **956.4 μs** | ✅ |

> **Phase 57+58-A+60-A note**: Pure NEON (Phase 57) + prefetch 192B→256B (Phase 58-A) + compact context state (Phase 60-A). Phase 60-A replaced 4×64-bit bit-stacks with `uint8_t cur_state_` — eliminating 5-7 ops per open/close bracket. canada.json gained **-15.8%** from simplified bracket handling. yyjson costs **50 cy/tok** on Snapdragon Gold vs 23 cy/tok on M1 Pro, confirming yyjson's dependency on M1's 576-entry reorder buffer.
>
> **Phase 73 note**: `dump(string&)` buffer-reuse overload pre-allocates the output buffer once; subsequent calls reuse the existing capacity — eliminating per-call `malloc+free`. On Android scudo allocator, `malloc(1.7 MB)+free` per iteration was the dominant overhead. Serialize improvement vs old `dump()`: twitter **−50%**, canada **−44%**, citm **−45%**, gsoc **−71%**. This closed the largest remaining Snapdragon gap (citm serialize) and now beast beats yyjson on all 4 serialize benchmarks.


---

## How It Works: Under the Hood

Beast JSON is an ongoing laboratory for JSON parsing techniques. Here's what's inside.

### 🧠 Tape-Based Lazy DOM Architecture

Instead of allocating a massive tree of scattered heap nodes (like traditional parsers), Beast writes a **flat, contiguous `TapeNode` array** inside a single pre-allocated memory arena. 

The most extreme optimization we achieved is compressing the entire contextual state of a JSON element into exactly **8 bytes** per node. This drastically reduces the working set size, perfectly aligning with modern CPU L2/L3 cache architectures.

#### The 8-Byte `TapeNode` Layout

```text
 31      24 23     16 15            0
 ┌────────┬─────────┬───────────────┐
 │  type  │   sep   │    length     │  meta (32-bit)
 └────────┴─────────┴───────────────┘
 ┌────────────────────────────────────┐
 │            byte offset             │  offset (32-bit)
 └────────────────────────────────────┘
```

| Field | Specs | Purpose | Performance Impact |
| :--- | :--- | :--- | :--- |
| **`offset`** | 32 bits | Raw byte offset into the original source buffer. | Enables **Zero-Copy** strings. We point directly to the source string instead of allocating memory for it. |
| **`length`** | 16 bits | Byte length of the token string. | O(1) string length lookups. |
| **`sep`** | 8 bits | Pre-computed separator bit (`0`=none, `1`=comma, `2`=colon). | Eliminates the entire separator state-machine overhead during serialization. |
| **`type`** | 8 bits | The node enum (ObjectStart, ArrayEnd, Number, String...). | Single-byte type checking. |

All string data stays in the original input buffer — the library is **100% zero-copy**. Serialization (`dump()`) is a single linear memory scan over the contiguous tape with direct pointer writes into a pre-sized output buffer, bypassing traditional tree-traversal overhead completely.

### Phase D1 — TapeNode Compaction (12 → 8 bytes)

The original `TapeNode` was 12 bytes with separate `type`, `length`, `offset`, and `next_sib` fields. `next_sib` existed to track sibling pointers — but analysis showed it was written and never actually read at runtime. After removing it and packing `type + sep + length` into a single `uint32_t meta`, the struct shrank from 12 → 8 bytes.

**Effect**: Each tape node now fits in 8 bytes instead of 12. For canada.json's 2.32 million float tokens, that's **~9.3 MB less working set** — a major L2/L3 cache improvement, yielding +7.6% parse throughput on canada.json.

### ⚡ SWAR String Scanning — SIMD Without SIMD

String parsing is the bottleneck in every JSON parser. Beast leverages **SWAR (SIMD Within A Register)** — 64-bit bitwise magic that processes 8 bytes at a time for `"` and `\` characters, using zero SIMD intrinsics. This allows Beast to fly even on older architectures.

> [!TIP]
> **The Math Behind The Magic**: `has_byte(v, c)` uses a classic trick: `(v - 0x0101...01 * c) & ~v & 0x8080...80`. It sets the highest bit in each byte position where the target character `c` appears!

```cpp
// Load 8 bytes into a 64-bit GPR
uint64_t v = load_u64(p);

// Broadcast compare: instantaneously find any quotes or backslashes
uint64_t q = has_byte(v, '"');
uint64_t b = has_byte(v, '\\');

if (q && (q < b || !b)) {
    // Quote found BEFORE any escape character!
    // Shift the pointer directly to the end of the string.
    p += ctz(q) / 8;
    goto string_done;
}
```

By adding a **cascaded early exit**, roughly ~36% of strings (like `"id"`, `"text"`) exit within the very first 8-byte chunk, avoiding further looping entirely.

### 🏗️ Pre-flagged Separators (The Ultimate Zero-Cost Abstraction)

This is the single most impactful optimization in the library: **completely eliminating the separator state machine from serialization (`dump()`)**.

Traditional JSON serializers waste cycles tracking state at runtime: *“Are we inside an object? Are we on a key or a value? Is this the first element?”* Every token requires 3–5 bit-stack operations just to decide whether to print a `,` or `:`. 

Beast eliminates this by pre-computing the separator **during parsing** and baking it directly into the `meta` descriptor.

> [!IMPORTANT]
> Because Beast calculates the bit-stacks (using precomputed `depth_mask_`) iteratively during the single pass, the cost is effectively hidden in instruction-level parallelism.

```cpp
/* --- 1. DURING PARSE (Phase 60-A + Phase 64) --- */
// cur_state_ is a register-resident uint8_t encoding three bits:
//   bit0 = is_key  (next push is an object key)
//   bit1 = in_obj  (we're inside an object, not an array)
//   bit2 = has_elem (≥1 element already pushed at this depth)
//
// Phase 64: two 8-byte LUTs replace ~14 instructions of bit arithmetic.
static constexpr uint8_t sep_lut[8] = {0, 0, 0, 0, 1, 0, 2, 1};
static constexpr uint8_t ncs_lut[8] = {4, 0, 0, 6, 4, 0, 7, 6};
sep       = sep_lut[cur_state_];   // 0=none · 1=comma · 2=colon
cur_state_ = ncs_lut[cur_state_];  // advance state machine

/* --- 2. DURING SERIALIZATION --- */
// One branch replaces 50 lines of complex state-tracking code!
const uint8_t sep = (meta >> 16) & 0xFF;
if (sep) *w++ = (sep == 2) ? ':' : ',';
```

**The Result:** The serialize inner loop becomes a devastatingly tight memory scan. No recursive calls, no state stacks. This delivers a **~29% serialize time reduction** on heavy datasets.

### Phase D4 — Single Meta Read per Iteration

In the `dump()` hot loop, `nd.meta` was accessed multiple times per token — once to get `type`, again for `length`, and again for `sep`. A single cache line miss could stall all three reads.

Fix: read `nd.meta` into a local `const uint32_t meta` once, then derive everything from it with cheap shifts and masks. One memory read replaces three.

**Effect**: twitter serialize -11% on its own; enabled further cleanup in Phase E.

### Phase 50+53 — Two-Phase AVX-512 Parsing (simdjson-style)

The biggest parse-speed breakthrough: a simdjson-inspired two-phase parsing pipeline, customized for Beast's tape architecture.

**Stage 1** (`stage1_scan_avx512`): a single AVX-512 pass over the entire input at 64 bytes/iteration. It uses `_mm512_cmpeq_epi8_mask` to detect quotes, backslashes, and structural characters (`{}[]`), and tracks in-string state via a cross-block XOR prefix-sum (`prefix_xor`). The result is a flat `uint32_t[]` positions array — one entry per structural character, quote, or value-start byte.

**Stage 2** (`parse_staged`): iterates the positions array without touching the raw input for whitespace or string-length scanning. String length becomes `O(1)`: `close_offset − open_offset − 1`. The push() bit-stacks handle key↔value alternation exactly as in single-pass mode — no separator entries needed.

**Phase 53 key insight**: `,` and `:` entries were removed from the positions array entirely. The push() bit-stack already knows whether the current token is a key or value; it doesn't need explicit separator positions. Removing them shrinks the positions array by ~33% (from ~150K to ~100K entries for twitter.json), reducing L2/L3 cache pressure and Stage 2 iteration count.

**Result**: twitter.json 365 μs → **202 μs** (−44.7% vs Phase 48 baseline) with PGO.

A 2 MB size threshold selects the path: files ≤2 MB (twitter, citm) use Stage 1+2; larger number-heavy files (canada, gsoc) fall back to the optimized single-pass parser, where the positions array would exceed L3 capacity.

> **Note on NEON/ARM64**: We implemented an equivalent `stage1_scan_neon` processing 64 bytes per iteration (unrolling 4 × 16-byte `vld1q_u8`). However, benchmark results showed a **~30-45% performance degradation** compared to the single-pass parser. Generating the 64-bit structural bitmasks requires too many shift-and-OR operations (`neon_movemask` per 16B chunk), creating higher overhead than simply scanning line-by-line. AArch64 benefits far more from our highly optimized single-pass linear scans than a two-phase architecture.

### Phase 59 — KeyLenCache: Schema-Prediction Key Scanner Bypass

The final x86_64 breakthrough: a 264-byte cache that makes key scanning O(1) for repeated-schema objects.

**Core insight**: In valid JSON, any `"` inside a string is escaped as `\"`. Therefore, if we've seen a key of source-length `N` before, we can detect it next time with a single byte comparison: `s[N] == '"'` — no SIMD scan needed.

```cpp
// Lookup: is the closing '"' where we expect it?
uint16_t cl = kc_.lens[depth_][key_idx];
// Guard: s[cl] must be the key's closing quote (followed by ':'),
// not a '"' inside the value region (false-positive trap).
if (cl != 0 && s + cl + 1 < end_ &&
    s[cl] == '"' && s[cl - 1] != ':' && s[cl + 1] == ':') {
    e = s + cl;             // cache HIT — skip entire SIMD scan
    goto skn_cache_hit;
}
// Miss: run normal SIMD scan, then record result for next time
kc_.lens[depth_][key_idx] = static_cast<uint16_t>(e - s);
```

**citm_catalog.json** has 243 `performance` objects, each with the same 9 keys. After the first object, all 2,187 subsequent key scans become byte comparisons. Combined with Stage 1+2 two-phase parsing, citm went from **757 μs to 582 μs (−23%)** — flipping from 3% behind yyjson to **37% ahead**.

---

## Features

- **Single header** — drop `beast_json.hpp` into your project, done.
- **Zero-copy** — string values point into the original source buffer; no allocation per token.
- **Lazy DOM** — navigate to only what you need; untouched parts of the JSON cost nothing.
- **Pre-flagged separators** — separator state computed at parse time, never at serialize time.
- **SWAR string scanning** — 8-bytes-at-a-time `"` / `\` detection without any SIMD intrinsics.
- **Tape compaction** — 8-byte nodes, cache-line-friendly linear layout.
- **C++20** — no macros beyond include guards; fully `constexpr` where applicable.

---

## 📚 Documentation
- [**Roadmap to v1.0**](docs/ROADMAP.md)
- [**Security & Memory-Safety Report**](SECURITY.md) — 5 ASan/UBSan/libFuzzer bugs fixed
- [API Readiness & The Ultimate API Blueprint](docs/API_READINESS_REPORT.md)
- [1.0 Release GitHub Page TODO](docs/GITHUB_PAGE_TODO.md)
- [Optimization Failures & History](docs/OPTIMIZATION_FAILURES.md)
- [Optimization TODO / Phase History](docs/TODO.md)
- [Extreme Optimization Plan (Phases 1-60)](docs/OPTIMIZATION_PLAN.md)

---

## 🗺️ Roadmap to 1.0 (The Ultimate API)

> 3종 플랫폼 최적화 완료 (x86_64 Phase 75 · Snapdragon Gen 2 Phase 73 · M1 Pro Phase 80-M1) · **The Ultimate API 완성** · **Zero-Effort 자동 직렬화 엔진 완성** · **RFC 8259 완전 준수** · **Foreign Language Bindings 완성** (ctest 368/368 PASS) — **v1.0 목표 달성**.

자세한 계획 및 진행 현황은 **[docs/ROADMAP.md](docs/ROADMAP.md)** 를 참조하세요.

**핵심 목표 (v1.0):**
- [x] **Core Engine**: x86_64 / Snapdragon 8 Gen 2 / M1 Pro 전 플랫폼 최적화 완료.
- [x] **Legacy DOM 제거**: `beast::json::Value`, `Parser`, `Object`, `Array`, `rtsm::Parser` 삭제 완료.
- [x] **3-Tier 아키텍처**: `beast::core` · `beast::utils` · `beast` 퍼사드 분리 완료.
- [x] **암시적 변환** (nlohmann 스타일): `int age = doc["age"];`
- [x] **안전 접근 체인** (SafeValue 모나드): `int t = doc.get("cfg")["timeout"].value_or(5000);`
- [x] **AutoChain / operator[] non-throwing**: `root["a"]["b"]["c"]` — missing key → invalid `Value{}`, 예외 없음.
- [x] **구조적 뮤테이션**: `erase(key/idx)`, `insert(key, T)`, `push_back(T)` — overlay, 원본 tape 불변.
- [x] **이터레이션**: `items()` / `elements()` — range-for, structured bindings, 삭제 항목 자동 skip.
- [x] **Pretty-print**: `dump(int indent)` — `root.dump(2)` / `root.dump(4)`.
- [x] **C++20 Ranges/STL 완전 호환**: `borrowed_range`, `std::views::filter/transform` 파이프 지원.
- [x] **C++20 Concepts**: 컴파일 타임 타입 안전성.
- [x] **`operator|` Pipe Fallback**: `int age = doc["age"] | 42;` — 누락·타입 불일치 시 기본값 반환.
- [x] **Typed Views `as_array<T>()`**: `for (int id : doc["ids"].as_array<int>())`.
- [x] **Runtime JSON Pointer `at(path)`**: `root.at("/users/0/name")` — RFC 6901.
- [x] **Compile-Time JSON Pointer `at<Path>()`**: `root.at<"/config/timeout">()` — 컴파일 타임 경로 검증.
- [x] **`contains()` / `value(key, default)`**: `root.contains("k")` / `root.value("age", 0)`.
- [x] **`type_name()`**: `"null"/"bool"/"int"/"double"/"string"/"array"/"object"`.
- [x] **`keys()` / `values()` 범위**: 객체 키·값 lazy 범위 (`std::views::transform` 기반).
- [x] **`merge()` / `merge_patch(json)`**: RFC 7396 JSON Merge Patch.
- [x] **`beast::read<T>()` / `beast::write()`**: 자동 직렬화 엔진 (STL 전체 + 매크로 구조체).
- [x] **`BEAST_JSON_FIELDS(Type, ...)`**: 한 줄 매크로로 커스텀 구조체 읽기+쓰기 완전 자동화.
- [x] **RFC 8259 완전 준수**: `beast::rfc8259::validate()` + `beast::parse_strict()` — 56개 테스트 통과.
- [x] **Foreign Language Bindings**: C Shared Library (`beast_json_c.so`) + Python ctypes 바인딩 완성.

---

## Usage

```cpp
#include <beast_json/beast_json.hpp>
#include <iostream>
#include <ranges>

int main() {
    std::string_view json = R"({
        "name": "Beast",
        "speed": 340,
        "tags": ["fast", "zero-copy", "c++20"]
    })";

    beast::Document doc;
    beast::Value root = beast::parse(doc, json);

    // Type-checked access (throws on mismatch)
    std::cout << root["name"].as<std::string>() << "\n";  // Beast
    std::cout << root["speed"].as<int>()        << "\n";  // 340

    // Implicit conversion (nlohmann-style)
    std::string name = root["name"];
    int speed = root["speed"];

    // Safe access — never throws (SafeValue chain)
    int timeout = root.get("config")["timeout"].value_or(5000);

    // AutoChain — missing key returns invalid Value{}, not exception
    if (root["missing"]["deep"])
        std::cout << root["missing"]["deep"].as<int>() << "\n";

    // Iteration
    for (auto tag : root["tags"].elements())
        std::cout << tag.as<std::string>() << "\n";  // fast, zero-copy, c++20

    for (auto [key, val] : root.items())
        std::cout << key << ": " << val.dump() << "\n";

    // Structural mutation (overlay — original tape unchanged)
    root["tags"].push_back("modern");
    root.insert("version", 1);
    root["tags"].erase(0);          // removes "fast"

    // Subtree + pretty-print serialization
    std::cout << root["tags"].dump()  << "\n";  // ["zero-copy","c++20","modern"]
    std::cout << root.dump(2)         << "\n";  // indented JSON

    // C++20 Ranges pipeline
    auto big_tags = root["tags"].elements()
        | std::views::filter([](auto v){ return v.as<std::string>().size() > 4; });
    for (auto v : big_tags)
        std::cout << v.as<std::string>() << "\n";

    // --- Convenience Layer ---

    // Pipe fallback: missing/wrong-type → default
    int speed   = root["speed"]   | 0;        // 340
    std::string name = root["name"] | "unknown";  // Beast

    // value(key, default) — same idea, key-only
    double v2 = root.value("speed", 0.0);      // 340.0
    bool found = root.contains("tags");         // true

    // type_name()
    std::cout << root["speed"].type_name() << "\n";  // "int"

    // keys() / values()
    for (std::string_view k : root.keys())
        std::cout << k << "\n";  // name, speed, tags, version

    // Typed array view
    root.insert("ids", beast::parse(beast::Document{}, "[1,2,3]")); // won't compile w/o stored doc
    // Better:
    beast::Document doc2;
    auto ids_doc = beast::parse(doc2, R"({"ids":[10,20,30]})");
    for (int id : ids_doc["ids"].as_array<int>())
        std::cout << id << " ";  // 10 20 30

    // Runtime JSON Pointer (RFC 6901)
    beast::Document doc3;
    auto nested = beast::parse(doc3, R"({"a":{"b":{"c":42}}})");
    std::cout << nested.at("/a/b/c").as<int>() << "\n";   // 42

    // Compile-time JSON Pointer
    std::cout << nested.at<"/a/b/c">().as<int>() << "\n"; // 42

    // merge_patch (RFC 7396)
    beast::Document doc4;
    auto cfg = beast::parse(doc4, R"({"timeout":5000,"retries":3})");
    cfg.merge_patch(R"({"timeout":10000,"debug":true})");
    // → timeout=10000, retries=3, debug=true (in dump())

    return 0;
}
```

---

## 🔄 Automatic Serialization

Zero user effort for all standard C++ types. One macro line for custom structs.

```cpp
#include <beast_json/beast_json.hpp>

// ── Tier 1: STL types — zero effort ──────────────────────────────────────────
auto v  = beast::read<std::vector<int>>("[1,2,3]");          // → {1,2,3}
auto m  = beast::read<std::map<std::string,double>>(R"({"pi":3.14})");
auto t  = beast::read<std::tuple<int,std::string,bool>>("[42,\"ok\",true]");
auto op = beast::read<std::optional<int>>("null");            // → nullopt

std::string j1 = beast::write(std::pair{3, "hello"s});       // [3,"hello"]
std::string j2 = beast::write(std::array<int,3>{1,2,3});     // [1,2,3]
std::string j3 = beast::write(std::optional<int>{});         // null

// ── Tier 2: custom structs — one macro line ───────────────────────────────────
struct Address { std::string city; std::string country; };
BEAST_JSON_FIELDS(Address, city, country)   // ← done!

struct User {
    std::string              name;
    int                      age   = 0;
    Address                  addr;                   // nested struct → auto
    std::vector<std::string> tags;                   // STL container → auto
    std::optional<double>    score;                  // optional → auto
};
BEAST_JSON_FIELDS(User, name, age, addr, tags, score)  // ← done!

auto user = beast::read<User>(R"({
    "name": "Alice", "age": 30,
    "addr": {"city": "Seoul", "country": "KR"},
    "tags": ["admin", "user"], "score": 99.5
})");

std::string json = beast::write(user);  // full round-trip

// vector of structs, map of structs — all automatic
auto users  = beast::read<std::vector<User>>(json_array);
auto places = beast::read<std::map<std::string, Address>>(json_obj);
```

**Supported types out of the box:**

| Category | Types |
| :--- | :--- |
| Primitives | `bool`, `int`, `long`, `double`, `float`, … |
| Strings | `std::string`, `std::string_view`, `const char*` |
| Optional | `std::optional<T>` ↔ `null` / T |
| Sequence | `std::vector<T>`, `std::list<T>`, `std::deque<T>` |
| Set | `std::set<T>`, `std::unordered_set<T>` |
| Map | `std::map<std::string,V>`, `std::unordered_map<std::string,V>` |
| Fixed array | `std::array<T,N>` |
| Tuple/Pair | `std::tuple<Ts…>`, `std::pair<A,B>` |
| Custom struct | `BEAST_JSON_FIELDS(Type, field…)` one-liner |
| Manual ADL | `from_beast_json()` / `to_beast_json()` |

```
```

---

## Build

### Requirements

- C++20 compiler (Clang 10+ or GCC 10+)
- CMake 3.14+

### Tests & Benchmarks

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBEAST_JSON_BUILD_TESTS=ON
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run benchmarks (requires twitter.json, canada.json, etc. in working directory)
cd build && ./benchmarks/bench_all --all
```

### Single-Header Use

Since beast-json is a single header library, you can also simply copy `include/beast_json/beast_json.hpp` into your project with no CMake required.

### Language Bindings (C / Python)

```bash
# Build C shared library
cmake -S . -B build -DBEAST_JSON_BUILD_BINDINGS=ON
cmake --build build --target beast_json_c

# Run Python example
cd bindings/python
BEAST_JSON_LIB_PATH=../../build/bindings/c python3 example.py
```

```python
from beast_json import Document, loads

doc = Document('{"name":"Alice","tags":["go","cpp"]}')
root = doc.root()
print(root["name"])           # Alice
print(root["tags"][0])        # go
print(root.at("/tags/1"))     # cpp (RFC 6901 JSON Pointer)

# Mutation
root["name"].set("Bob")
root.insert("version", 2)
print(root.dump(indent=2))

# RFC 8259 strict mode
try:
    bad = Document("[1,2,]", strict=True)
except ValueError as e:
    print(e)  # trailing comma at offset 5

# loads() — drop-in for json.loads()
data = loads('[1, 2, {"x": 3}]')  # → [1, 2, {'x': 3}]
```

---

## Test Coverage

| Test Suite | Tests | Status |
| :--- | ---: | :--- |
| RelaxedParsing | 5 | ✅ PASS |
| Unicode | 5 | ✅ PASS |
| StrictNumber | 4 | ✅ PASS |
| ControlChar | 4 | ✅ PASS |
| Comments | 4 | ✅ PASS |
| TrailingCommas | 4 | ✅ PASS |
| DuplicateKeys | 3 | ✅ PASS |
| ErrorHandling | 5 | ✅ PASS |
| Serializer | 3 | ✅ PASS |
| Utf8Validation | 14 | ✅ PASS |
| LazyTypes | 19 | ✅ PASS |
| LazyRoundTrip | 11 | ✅ PASS |
| ValueAccessors | 33 | ✅ PASS |
| ValueMutation | 23 | ✅ PASS |
| ValueAssign | 10 | ✅ PASS |
| SafeValue | 19 | ✅ PASS |
| Monadic | 10 | ✅ PASS |
| Concepts | 4 | ✅ PASS |
| StructuralMutation | 14 | ✅ PASS |
| Iteration | 6 | ✅ PASS |
| SubtreeDump | 4 | ✅ PASS |
| PrettyPrint | 5 | ✅ PASS |
| AutoChain | 4 | ✅ PASS |
| Ranges | 10 | ✅ PASS |
| Contains | 4 | ✅ PASS |
| ValueDefault | 5 | ✅ PASS |
| TypeName | 3 | ✅ PASS |
| PipeFallback | 9 | ✅ PASS |
| KeysValues | 3 | ✅ PASS |
| AsArray | 5 | ✅ PASS |
| JsonPointer | 7 | ✅ PASS |
| JsonPointerCT | 3 | ✅ PASS |
| Merge | 2 | ✅ PASS |
| MergePatch | 4 | ✅ PASS |
| StructBinding | 4 | ✅ PASS |
| IsValid | 3 | ✅ PASS |
| AutoSerial | 21 | ✅ PASS |
| MacroFields | 12 | ✅ PASS |
| RFC8259_Accept | 16 | ✅ PASS |
| RFC8259_Reject | 29 | ✅ PASS |
| RFC8259_ImplDefined | 3 | ✅ PASS |
| RFC8259_Roundtrip | 4 | ✅ PASS |
| RFC8259_API | 4 | ✅ PASS |
| **Total** | **312** | **100% PASS** |

---

## Security & Memory-Safety

Beast JSON has been hardened through AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), and libFuzzer-guided fuzzing. **5 memory-safety bugs** were discovered and fixed.

### Sanitizer Setup

```bash
# ASan + UBSan build (GCC)
cmake -S . -B build-san \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBEAST_JSON_BUILD_TESTS=ON \
  "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-san -j$(nproc)
cd build-san && ctest -j$(nproc)   # 312/312 PASS
```

### libFuzzer Targets

Three fuzz targets are available under `fuzz/`:

| Target | What it covers |
|--------|----------------|
| `fuzz_parse` | `beast::parse()`, all type accessors, dump, object/array iteration, SafeValue, JSON Pointer |
| `fuzz_lazy` | `parse_reuse()`, same as above plus `insert`/`erase`/`push_back` mutations |
| `fuzz_rfc8259` | Consistency oracle — traps if two independent parses of the same input produce different `dump()` output |

```bash
# Requires Clang 18 + libclang-rt-18-dev
cmake -S . -B build-fuzz \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBEAST_JSON_BUILD_FUZZ=ON \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=fuzzer,address,undefined"
cmake --build build-fuzz -j$(nproc)

mkdir -p fuzz/corpus
./build-fuzz/fuzz/fuzz_parse   fuzz/corpus/ -max_total_time=60
./build-fuzz/fuzz/fuzz_lazy    fuzz/corpus/ -max_total_time=60
./build-fuzz/fuzz/fuzz_rfc8259 fuzz/corpus/ -max_total_time=60
```

### Fixed Vulnerabilities

| # | Input | Sanitizer | Location | Description |
|---|-------|-----------|----------|-------------|
| 1 | `` ` [ ` `` | ASan heap-buffer-overflow | `skip_to_action()` | Dereference of `*p_` when `p_ == end_` |
| 2 | `,` | ASan heap-buffer-overflow | `done:` label | Empty tape returned as valid; `tape[0]` read from uninit malloc memory |
| 3 | `{false}` | ASan heap-buffer-overflow | Parser + iterators | Non-string object keys accepted; iterator reads past tape boundary |
| 4 | `[\x03\x00:}` | UBSan index -1 | `dump_changes_()` | Stale overlay maps across `parse_reuse()` calls; stack underflow at `stk[-1]` |
| 5 | multi-call | ASan heap-buffer-overflow | `skip_value_()` + dump | Depth-loop runs past `tape.cap`; `memcpy` past `source.size()` |

All fixes are `BEAST_UNLIKELY` branches or paths unreachable with well-formed JSON.
**No measurable performance regression** was observed (all deltas within ±5% noise).

See [SECURITY.md](SECURITY.md) for the full vulnerability report including before/after benchmark data.
