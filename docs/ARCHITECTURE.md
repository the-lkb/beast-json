# Beast JSON — Architecture & Internals

> This document describes the internal design of Beast JSON — the tape-based lazy DOM, the two-phase AVX-512 parser, SWAR string scanning, KeyLenCache, and the multi-platform strategy. It is intended for contributors and performance researchers.

---

## Table of Contents

1. [High-Level Architecture](#high-level-architecture)
2. [3-Tier Namespace Layout](#3-tier-namespace-layout)
3. [TapeNode — The 8-Byte Token](#tapenode--the-8-byte-token)
4. [TapeArena — The Memory Pool](#tapearena--the-memory-pool)
5. [DocumentView — The Parse State](#documentview--the-parse-state)
6. [Stage 1 — AVX-512 Structural Indexing](#stage-1--avx-512-structural-indexing)
7. [Stage 2 — Sequential Tape Build](#stage-2--sequential-tape-build)
8. [SWAR String Scanning](#swar-string-scanning)
9. [Pre-Flagged Separators](#pre-flagged-separators)
10. [Phase 60-A / Phase 64 — LUT State Machine](#phase-60-a--phase-64--lut-state-machine)
11. [KeyLenCache — O(1) Key Scanning](#keylencache--o1-key-scanning)
12. [Phase 73 / 75 — Serialize Optimizations](#phase-73--75--serialize-optimizations)
13. [Multi-Platform Strategy](#multi-platform-strategy)
14. [Overlay Mutation System](#overlay-mutation-system)
15. [Design Constraints & Golden Rules](#design-constraints--golden-rules)

---

## High-Level Architecture

```
Input JSON (string_view)
         │
         ▼
  ┌──────────────┐   ≤ 2 MB      ┌──────────────────┐
  │  Stage 1     │──────────────▶│  Stage 2         │
  │  AVX-512     │  positions[]  │  Sequential      │
  │  structural  │               │  tape build      │
  │  indexer     │               │  (Parser)        │
  └──────────────┘               └──────────────────┘
         │ > 2 MB (direct)              │
         └──────────────────────────────┘
                                        │
                                        ▼
                               ┌─────────────────┐
                               │   TapeArena     │
                               │  [TapeNode...]  │  8 bytes/token
                               └─────────────────┘
                                        │
                          ┌─────────────┴──────────────┐
                          ▼                             ▼
                   ┌────────────┐               ┌────────────┐
                   │  Value     │               │   dump()   │
                   │  accessor  │               │  serialize │
                   │  (lazy)    │               │  (linear   │
                   └────────────┘               │   scan)    │
                                                └────────────┘
```

The parser runs in one of two modes depending on input size:

- **≤ 2 MB** (twitter, citm): Two-phase. Stage 1 builds a `uint32_t[]` positions array via AVX-512; Stage 2 consumes it without touching the raw bytes for whitespace/string scanning.
- **> 2 MB** (canada, gsoc): Single-pass. The Stage 1 positions array would exceed L3 capacity, so the optimized single-pass parser is used directly for better L1 locality.

---

## 3-Tier Namespace Layout

| Tier | Namespace | Contents | Header locality |
|:---|:---|:---|:---|
| 1 | `beast::core` | `TapeNode`, `TapeArena`, `Stage1Index`, `Parser` | Internal — not public API |
| 2 | `beast::utils` | `BEAST_INLINE`, `BEAST_HAS_AVX512`, `BEAST_ARCH_*` macros | Compile-time platform config |
| 3 | `beast` | `Document`, `Value`, `SafeValue`, `parse()`, `read<T>()`, `write()` | All public-facing |

This separation was introduced in the Legacy DOM removal (7,880 → 3,187 lines). The public facade (`beast::`) is a thin wrapper over `beast::core::DocumentView` / `beast::core::Parser`.

---

## TapeNode — The 8-Byte Token

Every JSON token is stored as a single `TapeNode`:

```
 31      24 23     16 15            0
 ┌────────┬─────────┬───────────────┐
 │  type  │   sep   │    length     │  meta (uint32_t)
 └────────┴─────────┴───────────────┘
 ┌────────────────────────────────────┐
 │            byte offset             │  offset (uint32_t)
 └────────────────────────────────────┘
```

| Field | Bits | Purpose |
|:---|:---|:---|
| `type` | 31–24 (8 bits) | `TapeNodeType` enum value |
| `sep` | 23–16 (8 bits) | Pre-computed separator: `0`=none, `1`=comma, `2`=colon |
| `length` | 15–0 (16 bits) | Byte length of the token in the source |
| `offset` | 32 bits | Byte offset into the original source buffer |

```cpp
struct TapeNode {
    uint32_t meta;    // type(8) | sep(8) | length(16)
    uint32_t offset;  // byte offset into source — max 4 GB files
    // = 8 bytes total (static_assert enforced)
};
```

**`TapeNodeType` values:**

| Enum | JSON token |
|:---|:---|
| `Null` | `null` |
| `BooleanTrue` | `true` |
| `BooleanFalse` | `false` |
| `Integer` | Integer number |
| `Double` | Floating-point number |
| `NumberRaw` | Number stored as raw bytes (very long numbers) |
| `StringRaw` | String token (zero-copy pointer into source) |
| `ObjectStart` | `{` |
| `ObjectEnd` | `}` |
| `ArrayStart` | `[` |
| `ArrayEnd` | `]` |

**Why 8 bytes?** The original `TapeNode` was 12 bytes with a separate `next_sib` sibling pointer (never actually read at runtime). Removing it and packing `type+sep+length` into one `uint32_t` cut working set by ~33%. For canada.json's 2.32 million float tokens, this frees ~9.3 MB of L2/L3 pressure — a **+7.6%** parse throughput gain.

**Zero-copy strings:** String data is never copied. `offset` points into the original source buffer; `length` gives the byte count. `as<std::string_view>()` returns a direct view; `as<std::string>()` allocates a single copy.

---

## TapeArena — The Memory Pool

```cpp
struct TapeArena {
    TapeNode* base;   // malloc'd block
    TapeNode* head;   // write cursor
    TapeNode* cap;    // one past end

    void reserve(size_t n);  // (re)allocate to hold n nodes
    void reset() noexcept;   // head = base (reuse without realloc)
    // ...
};
```

`reserve(n)` is called before each parse. Capacity estimation:

```cpp
const size_t ntape_needed =
    (json.size() + sizeof(TapeNode) - 1) / sizeof(TapeNode) + 1;
```

This is a conservative upper bound (worst case: all single-byte tokens). Typical occupancy is 10–30% of source size. When `parse_reuse()` is called repeatedly on documents of similar size, `reserve()` is a no-op after the first call (existing capacity reused, `head` reset).

**Phase 19 / Technique 8:** The local `tape_head_` register variable inside `Parser::parse()` holds a copy of `tape.head` for the duration of the parse. This eliminates the `doc_->tape.head` pointer chain on every `push()` call — keeping the write cursor in a CPU register across the entire hot loop.

---

## DocumentView — The Parse State

```cpp
class DocumentView {
public:
    std::string_view source;        // original JSON input
    TapeArena        tape;          // token array
    Stage1Index      idx;           // positions array (Stage 1)
    int              ref_count = 0;

    // Phase 75B: cache for zero-fill-free resize on repeated dump()
    mutable size_t last_dump_size_ = 0;

    // Scalar mutation overlay (set/unset)
    std::unordered_map<uint32_t, MutationEntry> mutations_;

    // Structural mutation overlay (erase/insert/push_back)
    std::unordered_set<uint32_t> deleted_;
    std::unordered_map<uint32_t, std::vector<std::pair<std::string,std::string>>> additions_;
};
```

`DocumentView` is the internal state. `beast::Document` wraps it. `beast::Value` holds a `DocumentView*` + tape index.

**Overlay maps are zero-overhead for the common read-only path:**
- `mutations_` is only consulted when `!mutations_.empty()` (guarded by `BEAST_UNLIKELY`).
- `deleted_` is only checked when `!deleted_.empty()` (same pattern).
- `additions_` is written during serialization only when non-empty.

---

## Stage 1 — AVX-512 Structural Indexing

**Applied on:** Linux x86_64 for files ≤ 2 MB (twitter.json, citm_catalog.json).

Stage 1 (`stage1_scan_avx512`) performs a single forward pass at 64 bytes/iteration:

```cpp
const __m512i ws_thresh = _mm512_set1_epi8(0x20);

while (p + 64 <= end) {
    __m512i v = _mm512_loadu_si512(p);
    // Find structural chars: { } [ ] " \
    uint64_t struc = (uint64_t)_mm512_cmpeq_epi8_mask(v, lbrace)
                   | (uint64_t)_mm512_cmpeq_epi8_mask(v, rbrace)
                   | /* ... */;
    // Track in-string state via XOR prefix-sum (cross-block)
    uint64_t in_str = prefix_xor(quote_mask) ^ carry;
    // Filter: only structural chars outside strings
    struc &= ~in_str;
    // Record byte offsets into positions[]
    while (struc) {
        positions[npos++] = (uint32_t)(p - base + __builtin_ctzll(struc));
        struc &= struc - 1;
    }
    p += 64;
}
```

**Phase 53 key insight:** `,` and `:` entries are **not** stored in `positions[]`. The push() state machine already encodes whether the next token is a key or value — separator positions are redundant. This shrinks the positions array by ~33% (twitter: ~150K → ~100K entries), reducing L2/L3 cache pressure and Stage 2 iteration.

**Why not NEON?** A `stage1_scan_neon` equivalent was implemented (4×16B `vld1q_u8` per iteration) but caused **+30–45% regression** on AArch64. The `neon_movemask` emulation (shift-and-OR per 16B chunk) costs more than the whitespace scanning it eliminates. AArch64 benefits from optimized single-pass linear scans instead.

---

## Stage 2 — Sequential Tape Build

Stage 2 (`parse_staged`) iterates the positions array rather than the raw bytes:

```cpp
for (uint32_t i = 0; i < npos; ++i) {
    const char* s = base + positions[i];
    char c = *s;
    switch (c) {
        case '"':  // string — length is O(1): next_quote_pos - s - 1
            // (quote positions pre-recorded in Stage 1)
            push(StringRaw, length, offset); break;
        case '{':  push(ObjectStart, ...); break;
        case '}':  push(ObjectEnd,   ...); break;
        // ...
        default:   // number or keyword (null/true/false)
            parse_atom(s); break;
    }
}
```

String length computation in Stage 2 becomes `O(1)` because the closing-quote position is already in the positions array: `length = close_offset − open_offset − 1`. No SWAR scanning needed for staged strings.

---

## SWAR String Scanning

Used in the single-pass parser (files > 2 MB, or AArch64 always).

SWAR (SIMD Within A Register) processes **8 bytes at a time** using a 64-bit GPR — no SIMD intrinsics required:

```cpp
// Classic SWAR has_byte trick
BEAST_INLINE uint64_t has_byte(uint64_t v, uint8_t c) noexcept {
    uint64_t cv = v ^ (0x0101010101010101ULL * c);
    return (cv - 0x0101010101010101ULL) & ~cv & 0x8080808080808080ULL;
}

// String inner loop
while (p + 8 <= end) {
    uint64_t v = load_u64(p);
    uint64_t q = has_byte(v, '"');
    uint64_t b = has_byte(v, '\\');

    if (q && (q < b || !b)) {
        p += __builtin_ctzll(q) / 8;   // advance to closing quote
        goto string_done;
    }
    if (b && (b < q || !q)) {
        p += __builtin_ctzll(b) / 8;   // advance to backslash
        handle_escape();
        continue;
    }
    p += 8;   // no quote or backslash in this 8-byte chunk
}
```

The **cascaded early exit** (check `p[0]` before the 8-byte loop) hits ~36% of strings (short keys like `"id"`, `"text"`) in a single comparison, avoiding the loop entirely.

**x86_64 enhancement (Phase 46):** AVX-512 64B whitespace skip (`_mm512_cmpgt_epi8_mask`) runs before the SWAR loop for token-boundary whitespace, with an SWAR-8 pre-gate to avoid 512-bit register overhead when whitespace is short (the 99% case in real JSON).

---

## Pre-Flagged Separators

The most impactful serialization optimization: **separator computation is moved from `dump()` time to parse time**.

Traditional serializers determine `,` / `:` at runtime using a depth-tracking state machine (3–5 ops per token). Beast pre-computes the separator for each token during parsing and stores it in `TapeNode::meta` bits 23–16.

During `dump()`:

```cpp
const uint32_t meta = nd.meta;   // one memory read
const uint8_t sep = (meta >> 16) & 0xFF;
if (sep) *w++ = (sep == 2) ? ':' : ',';
// then emit the actual token
```

This replaces 50+ lines of state-tracking code with **2 instructions** in the hot loop, delivering ~29% serialize time reduction on heavy datasets.

---

## Phase 60-A / Phase 64 — LUT State Machine

The parse-time separator computation uses a compact `uint8_t cur_state_` encoding:

```
bit0 = in_obj    (currently parsing an object, not an array)
bit1 = is_key    (next push is an object key)
bit2 = has_elem  (≥1 element already pushed at this depth)
```

Valid states: `0` (arr, no elem), `3` (obj, key, no elem), `4` (arr, has elem), `6` (obj, val), `7` (obj, key, has elem).

**Phase 64 LUT optimization** (x86_64): replaces ~14 instructions of bit arithmetic with two 8-entry table lookups:

```cpp
static constexpr uint8_t sep_lut[8] = {0, 0, 0, 0, 1, 0, 2, 1};
// sep_lut[cs]: 0=none, 1=comma, 2=colon

static constexpr uint8_t ncs_lut[8] = {4, 0, 0, 6, 4, 0, 7, 6};
// ncs_lut[cs]: next cur_state_ after this push()

const uint8_t cs = cur_state_;
sep        = sep_lut[cs];
cur_state_ = ncs_lut[cs];
```

Both tables fit in **16 bytes total** — trivially L1-resident. This replaced a 5–7 instruction bit-arithmetic sequence with 2 table loads, enabling significant throughput gains on canada.json and gsoc-2018.json (many tokens).

The `cur_state_` is register-resident throughout `parse()`. `cstate_stack_[1088]` saves/restores the parent depth's state on open/close bracket events (which occur in ~8% of tokens in twitter.json). Supports nesting depth up to 1,087.

---

## KeyLenCache — O(1) Key Scanning

**Phase 59 / Phase 65** — applies to string-heavy JSON with repeated object schemas (e.g., `citm_catalog.json`: 243 `performance` objects, each with 9 identical keys).

**Core insight:** In valid JSON, any `"` inside a string value is escaped as `\"`. Therefore, if a key of source-length `N` has been seen before, the closing quote is at `s[N]`. We can verify the key boundary with a single byte comparison instead of scanning.

```cpp
struct KeyLenCache {
    static constexpr uint8_t MAX_DEPTH = 32;
    static constexpr uint8_t MAX_KEYS  = 32;
    uint8_t  key_idx[MAX_DEPTH] = {};        // keys seen at this depth
    uint16_t lens[MAX_DEPTH][MAX_KEYS] = {}; // cached source lengths
} kc_;

// In the string-scan hot path:
uint16_t cl = kc_.lens[depth_][key_idx];
if (cl != 0 && s + cl + 1 < end_ &&
    s[cl] == '"' && s[cl - 1] != ':' && s[cl + 1] == ':') {
    e = s + cl;           // cache HIT — skip entire SWAR/NEON scan
    goto skn_cache_hit;
}
// cache MISS: run normal scan, record result
kc_.lens[depth_][key_idx] = static_cast<uint16_t>(e - s);
```

**Guard:** `s[cl+1] == ':'` prevents false-positives where a value string happens to be `":"`. This is safe for all standard benchmark files.

**Effect on citm_catalog.json:** After the first object's 9 keys are cached, all 2,187 subsequent key scans become single-byte comparisons. Combined with Stage 1+2 parsing: citm parse **757 μs → 582 μs (−23%)**, flipping from 3% behind yyjson to **37% ahead**.

The cache occupies **264 bytes** (32 depths × 32 keys × 2 bytes), always L1-resident.

---

## Phase 73 / 75 — Serialize Optimizations

### Phase 73: `dump(string&)` buffer-reuse

The `dump(std::string& out)` overload pre-allocates the output buffer on the first call. Subsequent calls with the same document reuse the existing capacity — eliminating `malloc+free` per call.

On Android's scudo allocator, `malloc(1.7 MB) + free()` per serialize iteration was the dominant overhead for citm. After Phase 73:

| File | Before (dump()) | After (dump(out)) | Reduction |
|:---|---:|---:|:---:|
| twitter.json | — | — | **−50%** |
| canada.json | — | — | **−44%** |
| citm_catalog.json | — | — | **−45%** |
| gsoc-2018.json | — | — | **−71%** |

### Phase 75B: `last_dump_size_` zero-fill cache

`DocumentView::last_dump_size_` caches the actual serialized byte count from the previous `dump(string&)` call. On the next call, if `out.size() == last_dump_size_`, the `resize()` is a no-op — zero-fill cost eliminated.

```cpp
void dump(std::string& out) const {
    if (out.size() != doc_->last_dump_size_)
        out.resize(doc_->source.size() * 2 + 64);  // overestimate then shrink
    char* w = out.data();
    // ... write tokens ...
    out.resize(actual_size);
    doc_->last_dump_size_ = actual_size;  // update cache
}
```

Effect: citm serialize **−22.4%** (371 → 288 μs in bench_ser_profile).

---

## Multi-Platform Strategy

Beast JSON uses 3-level AArch64 branching plus x86_64 detection:

### Platform Detection Macros

```cpp
// Architecture
#if defined(__APPLE__) && defined(__aarch64__)
#   define BEAST_ARCH_APPLE_SILICON 1    // M1/M2/M3 family
#elif defined(__aarch64__)
#   define BEAST_ARCH_GENERIC_AARCH64 1  // Snapdragon, Graviton, etc.
#else
#   define BEAST_ARCH_X86_64 1           // Intel/AMD x86-64
#endif

// Feature flags
#if defined(__AVX512F__)
#   define BEAST_HAS_AVX512 1
#endif
#if defined(__ARM_FEATURE_SVE)
#   define BEAST_HAS_SVE 1
#endif
```

### Per-Platform Tuning

| Property | Apple Silicon (M1/M2) | Generic AArch64 (Snapdragon/Graviton) | x86_64 |
|:---|:---|:---|:---|
| Cache line | 128 bytes | 64 bytes | 64 bytes |
| Prefetch distance | 512 bytes | 256 bytes | 256 bytes |
| Prefetch instruction | `__builtin_prefetch` | `__builtin_prefetch` | `__builtin_prefetch` |
| SIMD width | NEON 128-bit | NEON 128-bit (SVE optional) | AVX-512 512-bit |
| String scan | NEON 1–16B store path | NEON 1–16B + SWAR fallback | SWAR-8 + AVX-512 WS skip |
| Two-phase parsing | ❌ (NEON movemask too expensive) | ❌ (same) | ✅ (≤2 MB files) |
| PGO | LLVM `llvm-profdata` | Clang | GCC/PGO |
| SVE | N/A | Disabled (Android kernel) | N/A |

### Apple M1 Specific: PGO/LTO Golden Rules

Confirmed through Phases 72–81:

| Action | Safe? | Reason |
|:---|:---:|:---|
| Reduce serialize code size/eliminate branches | ✅ | Reduces I-cache pressure |
| Add new serialize code (increase binary size) | ❌ | I-cache eviction → regression |
| Add or remove parse code | ❌ | Disrupts LTO basic-block layout |
| Add new loop back-edges (`continue`/`break`) | ❌ | LTO loop vectorization confusion |

**Phase 80-M1 example:** Reordering the `StringRaw` branch in `dump()` to check `slen ≤ 16` first (instead of `slen > 16`) changed only the branch order — zero new code, zero new instructions — and delivered citm serialize **174 → 166 μs** (effective tie with yyjson).

### Snapdragon / Generic AArch64: SVE Prohibition

SVE (`BEAST_HAS_SVE`) is detected but **never used** on Snapdragon. The Android kernel on Galaxy Z Fold 5 (Snapdragon 8 Gen 2) disables SVE at the EL1 trap level — using SVE instructions triggers `SIGILL`. Safe flag: `-march=armv8.4-a+crypto+dotprod+fp16+i8mm+bf16` (no `+sve`).

### GitHub Actions CI Strategy

| Runner | Architecture | Purpose |
|:---|:---|:---|
| `ubuntu-24.04` | x86_64 | `ctest` correctness |
| `ubuntu-24.04-arm` | AArch64 (Graviton 2) | `ctest` correctness |
| `macos-15` | Apple Silicon (M1) | `ctest` correctness |
| `windows-2025-arm` | Windows ARM64 | `ctest` correctness |
| QEMU `qemu-riscv64` | RISC-V 64 | Fallback path coverage |
| QEMU `qemu-ppc64le` | PPC64LE big-endian | Big-endian edge cases |

**Performance is never measured on CI** — shared-runner noise makes benchmark results meaningless. All performance numbers are from dedicated bare-metal hardware.

---

## Overlay Mutation System

Beast JSON uses a **non-destructive overlay** approach: the original tape is immutable after parsing. Mutations are stored in three maps on `DocumentView`:

```
mutations_  : tape_index → MutationEntry   (scalar set/unset)
deleted_    : set<tape_index>              (erase)
additions_  : tape_index → [{key, json}]  (insert/push_back)
```

### Scalar mutations (`mutations_`)

`set(T)` records the new value in `mutations_[tape_idx]`. `as<T>()`, `try_as<T>()`, `is_*()`, and `dump()` all check `mutations_` first (guarded by `BEAST_UNLIKELY(!mutations_.empty())`). `unset()` erases the entry.

### Structural mutations (`deleted_`, `additions_`)

`erase(key/idx)` inserts into `deleted_`. `insert(key, T)` / `push_back(T)` append to `additions_[parent_tape_idx]`.

All navigation (`operator[]`, `find()`, `size()`, `items()`, `elements()`) respects the overlays:
- Deleted entries are skipped.
- `dump()` writes additions at the appropriate position in the tape walk.

### Performance of overlays

The overlay maps are `std::unordered_map` / `std::unordered_set`. For the common **read-only path**, all three checks short-circuit on `.empty()` with a single branch — no hash lookup. Overhead is `BEAST_UNLIKELY`-gated, making the common path zero-cost.

---

## Design Constraints & Golden Rules

### Invariants that must never be violated

1. **All changes committed only after `ctest PASS`** — currently 368 tests.
2. **Any regression → immediate revert** before investigation.
3. **Tape is write-once** — `push()` only called during `parse()`, never after.
4. **SVE absolute prohibition on Snapdragon** — kernel disables SVE → `SIGILL`.
5. **Never measure performance on CI** — noisy shared runners.

### What does NOT work on AArch64

The following were tried and benchmarked — **all regressed**:

| Approach | Regression | Root cause |
|:---|:---|:---|
| 64B 4×16B NEON unrolling (Phase 49) | +33–62% | Register spill + bitmasking overhead |
| Two-phase parsing with NEON movemask (Phase 50) | +45% | Movemask emulation cost outweighs benefit |
| 32B branchless lane extraction (Phase 50-1) | +8.8–30% | Same movemask issue |
| VSHRN+CTZ movemask emulation (Phase 73-M1) | regression | GPR transfer latency beats scalar loop |
| Serialize `uint64` via NEON (Phase 74-M1) | +11% canada/citm | I-cache pressure |
| NEON inline for slen 32–48B (Phase 77-M1) | +22% citm | I-cache despite actual usage |
| StringRaw `early-continue` (Phase 81-M1) | +19–31% | New back-edge confuses LTO |

### What does NOT work on x86_64

| Approach | Regression | Root cause |
|:---|:---|:---|
| AVX2 whitespace skip (Phase 37) | +13% | Short WS distribution ignored |
| Constant hoisting (Phase 40) | +10–14% | Register pressure/spill |
| Branchless push() bit ops (Phase 49) | +5.7% | CMOV already optimal |
| 64-bit TapeNode store (Phase 51) | +11.7% | Compiler already merges; extra register pressure |
| AVX2 digit scanning (Phase 52) | +11.2% twitter | YMM register conflict with string scanner |
