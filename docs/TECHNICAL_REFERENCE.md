# qbuem-json — Technical Reference (v1.0)

> This document is the unified technical reference for qbuem-json. It combines architecture details, performance data, the optimization roadmap, API reference, and lessons learned from optimization failures.

[TOC]


## 1. Introduction

qbuem-json is a high-performance, header-only C++20 JSON parser and serializer. It operates on a **tape-based lazy DOM** and utilizes SIMD instructions (AVX-512, NEON) or SWAR (SIMD Within A Register) for peak performance. It is designed to be a drop-in single header library without dependencies.


## 2. Performance Benchmarks

All measurements are taken on dedicated bare-metal hardware. `yyjson` is the primary benchmark target.

### 2.1 Linux x86_64 (GCC 13.3, AVX-512, PGO)
* **Parse**: Beats `yyjson` by +21% to +121% on all standard files (Phase 75).
  * `twitter.json`: 189 μs (3.27 GB/s)
  * `gsoc-2018.json`: 731 μs (4.45 GB/s)
* **Serialize**: Up to 4.18× faster than `yyjson` on specific files (e.g., `canada.json` 789μs vs 3301μs).

### 2.2 Linux AArch64 (GCC/Clang)
* **Parse & Serialize**: Completely sweeps `yyjson` (Phase 73).
  * Parse: +67% to +153% faster.
  * Serialize: 2.2× to 5.6× faster.

### 2.3 Apple Silicon (macOS, Apple Clang, NEON, PGO)
* **Serialize**: Sweeps `yyjson` (Phase 80-M1). Up to 4.2× faster (`gsoc-2018.json`).
* **Parse**: Trails `yyjson` slightly on 3/4 files due to architectural limits (Apple Silicon's extreme 576-entry ROB favors flat arrays over tape indirection).

### 2.4 Sub-MegaByte Memory Efficiency
qbuem-json uses a compact 8-byte `TapeNode` and zero-copy string references to achieve the lowest memory overhead in the C++ ecosystem. Measured via MacOS `mach_task` Resident Set Size (RSS) parsing `twitter.json` (631.5 KB):
* **qbuem_json**: 2.76 MB Peak RSS (DOM Size: 236 KB / 0.37x overhead)
* **simdjson**: 3.49 MB Peak RSS
* **yyjson**: 3.58 MB Peak RSS
* **Glaze**: 4.29 MB Peak RSS

### 2.5 Extreme Heavy-Load Benchmarks (Harsh Environment)
Performance under extreme stress: measuring a massive 5.5MB file containing 50,000 deeply nested objects, arrays, floats, and heavily escaped strings (containing `\n`, `\t`, `\r`, and escaped quotes). This tests the parser's absolute worst-case fallback performance.

| Library | Parse Time | Serialize Time | Overall Edge |
|:---|---:|---:|:---:|
| **qbuem-json** | 5.28 ms | **2.32 ms** | **Fastest Serialization** |
| `simdjson` | **4.89 ms** | 12.16 ms | Fastest Parse |
| `yyjson` | 5.29 ms | 3.53 ms | - |
| `RapidJSON` | 13.15 ms | 11.90 ms | - |
| `Glaze DOM`| 35.96 ms | 10.69 ms | Dynamic-type penalty |
| `nlohmann` | 58.94 ms | 14.93 ms | - |


## 3. Architecture & Internals

qbuem-json stores every JSON token as a flat `TapeNode` array (8 bytes/node) inside a pre-allocated `TapeArena`.

### 3.1 TapeNode Layout
```text
 31      24 23     16 15            0
 ┌────────┬─────────┬───────────────┐
 │  type  │   sep   │    length     │  meta (32-bit uint)
 └────────┴─────────┴───────────────┘
 ┌────────────────────────────────────┐
 │            byte offset             │  offset (32-bit uint)
 └────────────────────────────────────┘
```
1. **Zero-Copy Strings**: `offset` points directly into the original input buffer.
2. **Pre-Flagged Separators**: The `sep` field stores the `,` or `:` separator at parse time, allowing the serializer to avoid state-machine tracking completely.

### 3.2 Two-Phase Parser (x86_64 <= 2MB)
1. **Stage 1 (AVX-512)**: Scans 64 bytes at a time, building an array of structural token positions.
2. **Stage 2 (Sequential)**: Iterates the positions array, skipping whitespace instantly and computing string lengths in O(1) time.

### 3.3 SWAR String Scanning
For files > 2MB or on AArch64, qbuem-json uses a 64-bit GPR SWAR scan (8 bytes/cycle) to find quotes or escape characters without heavy SIMD overhead.

### 3.4 KeyLenCache
For repeated object schemas (e.g., `citm_catalog.json`), qbuem-json caches the length of keys seen at specific depths. Once cached, scanning a key becomes a single-byte `O(1)` comparison.


## 4. API Reference

### 4.1 `qbuem::Document` and `qbuem::Value`
`qbuem::Document` owns the tape. `qbuem::Value` is the 16-byte handle used for navigation.
```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({"score": 9.9})");

// Typed Access (Throws on mismatch)
double s = root["score"].as<double>();

// Implicit conversion
double s2 = root["score"];

// Pipe Fallback (Never throws)
double s3 = root["score"] | 0.0;
```

### 4.2 Non-Destructive Mutations
Tape is immutable. Mutations use overlay maps (`mutations_`, `additions_`, `deleted_`, `array_insertions_`).
```cpp
root["score"].set(10.0);           // Scalar override (mutations_ map)
root.insert("active", true);       // Structural addition (additions_ map)
root.erase("old_key");             // Structural deletion (deleted_ set)
root["tags"].push_back("cpp20");   // Array append (array_insertions_ map)
std::cout << root.dump();          // Reflects all overlay mutations
```

**`unset()` semantics**: `unset()` removes only the scalar overlay from `mutations_`. It reverts the value to the **original parsed tape entry** — it does NOT set the value to null. Structural additions (`insert()`, `push_back()`) are unaffected.

**`size()` for arrays**: Returns `tape_count + push_back_additions` — reflects both the original parsed elements and elements appended via `push_back()`.

**`items()` for objects**: Iterates tape keys first, then keys added via `insert()` — so all structural additions are visible.

### 4.3 Iteration and C++20 Ranges
```cpp
for (auto [key, val] : root.items()) { /* tape keys + insert() keys */ }
for (auto elem : root["tags"].elements()) { /* array items */ }

// C++20 views
auto big = root["scores"].elements() | std::views::filter([](auto v){ return v.as<int>() > 3; });
```

### 4.4 Unsigned Integer Overloads
`operator[](index)` and `erase(idx)` on `Value` and `SafeValue` accept both `size_t` and `unsigned int`, eliminating the need for explicit casts when using unsigned literals:
```cpp
auto elem = root["array"][0u];   // unsigned int literal — no cast needed
root["array"].erase(0u);         // also unsigned int
```
Plain `int` still requires a cast to `size_t` to avoid ambiguity with the `const char*` overload.


## 5. Auto-Serialization Macro

Zero-boilerplate serialization utilizing C++20 Concepts. Built-in support for all STL containers.

```cpp
struct User {
    std::string name;
    int age = 0;
    std::optional<double> score;
};
QBUEM_JSON_FIELDS(User, name, age, score)

auto user = qbuem::read<User>(R"({"name": "Alice"})");
std::string json = qbuem::write(user);
```

### 5.1 Custom Third-Party Types via ADL
If a type cannot be modified to use `QBUEM_JSON_FIELDS` (e.g., a third-party struct like `glm::vec3`), you can opt into auto-serialization by defining two Argument-Dependent Lookup (ADL) functions in the same namespace as the type:

```cpp
#include <glm/vec3.hpp>

namespace glm {
    // 1. Define from_qbuem_json for parsing (must be in the same namespace)
    inline void from_qbuem_json(const qbuem::json::Value& v, vec3& out) {
        // Read directly from the JSON array
        out.x = v[0].as_double();
        out.y = v[1].as_double();
        out.z = v[2].as_double();
    }

    // 2. Define to_qbuem_json for serialization
    inline void to_qbuem_json(qbuem::json::Value& root, const vec3& in) {
        // Construct the expected structure (e.g., a JSON array)
        root = qbuem::json::Value::Array();
        root.push_back(qbuem::json::Value(in.x));
        root.push_back(qbuem::json::Value(in.y));
        root.push_back(qbuem::json::Value(in.z));
    }
}
```
`qbuem::read<T>` and `qbuem::write(T)` natively search for these ADL hooks during compile-time resolution.


## 6. RFC 8259 Validator

Strict validation mode that throws an exception with offset details on RFC violations (trailing commas, leading zeros, etc.).
```cpp
qbuem::Document doc;
auto root = qbuem::parse_strict(doc, "[1, 2,]"); // Throws std::runtime_error
```


## 7. Language Bindings

qbuem-json provides a C API and a Python `ctypes` wrapper.

### Python Example
```python
from qbuem_json import Document, loads
doc = Document('{"name": "Alice"}')
print(doc.root()["name"])
```


## 8. Optimization Failures & Lessons

Optimization attempts that caused performance regressions provide vital architectural insights.

### 8.1 AArch64
* **Rule**: NEON is supreme. Never mix scalar `while` loops or SWAR with NEON fast-paths (e.g., for pre-gating). GPR-to-SIMD branch dependence breaks the pipeline.
* **Rule**: 32B/64B manual unrolling degrades I-cache without improving ILP on AArch64. 16B is the sweet spot.
* **Rule**: Two-phase parsing using NEON movemask emulation is far slower than single-pass linear scanning on AArch64.

### 8.2 x86_64
* **Rule**: AVX2 constant hoisting increases register pressure and causes spills. Declare `_mm256_set1` immediately near use.
* **Rule**: Overlapping SIMD intensive loops (e.g., AVX2 digit parsing and AVX2 string parsing) in the same function breaks YMM register limits causing spills.

### 8.3 Apple Silicon PGO/LTO Golden Rules
* **Rule**: Adding *any* new loop back-edges (`continue`, `break`) confuses LTO optimization.
* **Rule**: Code size additions in serialize functions increase I-cache pressure and cause regressions in parse performance due to LTO layout changes.


## 9. Development Roadmap & History

qbuem-json achieved v1.0 goals entirely through an AI-driven optimization pipeline crossing 80+ distinct phases.

* **Phase 1-40**: Legacy DOM creation and baseline parser implementation.
* **Phase 44-53 (x86_64)**: AVX-512 integration, Stage 1/2 parsing, yielding massive parse throughput gains.
* **Phase 57-64 (AArch64)**: Perfecting the Pure NEON pipeline and compact state machines.
* **Phase 65**: KeyLenCache for O(1) object key lookups.
* **Phase 72-81 (Apple Silicon)**: Uncovering extreme PGO/LTO sensitivities and completing the fastest JSON serializer for Apple hardware.
* **Phase 82-100+**: Legacy DOM removal, introduction of Monadic `SafeValue`, C++20 modernizations, and final v1.0 stabilization.


## 10. Security & Memory-Safety Hardening

> **Summary**: 5 memory-safety vulnerabilities were discovered through AddressSanitizer (ASan),
> UndefinedBehaviorSanitizer (UBSan), and libFuzzer-guided fuzzing, then fixed.
> All fixes are defence-in-depth guards on malformed / adversarial input;
> they have **no measurable performance impact** on well-formed JSON.

### 10.1 Vulnerability Report

#### Bug 1 — Null-Dereference in `skip_to_action()` *(heap-buffer-overflow)*
* **Trigger**: Input `[` (unterminated array, 1 byte)
* **Root cause**: `skip_to_action()` dereferenced `*p_` without checking `p_ < end_` first. Any input whose last meaningful byte is `[` or `{` causes the parser to re-enter `skip_to_action()` with `p_ == end_`.
* **Fix**: Added `if (QBUEM_UNLIKELY(p_ >= end_)) return 0;` as the very first statement in `skip_to_action()`.

#### Bug 2 — Empty-Tape Read via Bare Separator *(heap-buffer-overflow)*
* **Trigger**: Input `,` (single comma, 1 byte)
* **Root cause**: `kActComma` / `kActColon` consumed bytes without writing any tape node. `depth_ == 0` caused `parse()` / `parse_staged()` to return `true` with an empty tape. Subsequent `tape[0]` access read from uninitialized `malloc` memory.
* **Fix**: The `done:` label return condition in both `parse()` and `parse_staged()` now requires `tape_head_ > doc_->tape.base` in addition to `depth_ == 0`.

#### Bug 3 — Non-String Object Keys + Iterator Out-of-Bounds *(heap-buffer-overflow)*
* **Trigger**: Inputs `{false}`, `{]\x01...`
* **Root cause**: The parser's non-string value cases lacked an `is_key` state check. `skip_val_s_()` and iterators accessed `doc_->tape[i]` without a tape-size bounds check.
* **Fix**: Added `if (QBUEM_UNLIKELY(cur_state_ & 0b001u)) goto fail;` to all six non-string value cases. Added `tape_sz` bounds guard at the top of iterators and in `skip_val_s_()`.

#### Bug 4 — Stale Overlay Maps + Stack Underflow in `dump_changes_()` *(UBSan / stack-buffer-underflow)*
* **Trigger**: `[\x03\x00:}` fed to `fuzz_lazy` after a prior call that performed mutations.
* **Root cause**: `parse_reuse()` did not clear `mutations_`, `deleted_`, or `additions_` between calls. `dump_changes_()` accessed the stack when `top == -1`.
* **Fix**: Added `.clear()` for all three overlay maps at the start of `parse_reuse()`. Added `if (QBUEM_UNLIKELY(top < 0))` early-exit guards.

#### Bug 5 — `skip_value_()` Out-of-Bounds + `memcpy` Past Source End *(heap-buffer-overflow)*
* **Trigger**: Multi-invocation sequence with static `g_doc`; intermediate failed parses leave stale tape content.
* **Root cause**: `skip_value_()` depth-tracking loop accessed `doc_->tape[idx]` with no bounds check. `dump_subtree_()` used `nd.offset + len` without verifying via `doc_->source.size()`.
* **Fix**: `skip_value()_` added bounds check. `dump_substree_()` added a clamping: `std::min(len, source.size() - nd.offset)`.

### 10.2 Fuzz Infrastructure

Three libFuzzer targets (Clang 18, static ASan+UBSan, `-fsanitize=address,undefined`):
1. `fuzz/fuzz_parse.cpp`: `qbuem::parse()`, typing, iterators, JSON Pointer
2. `fuzz/fuzz_lazy.cpp`: same + `insert` / `erase` / `push_back` mutations
3. `fuzz/fuzz_rfc8259.cpp`: Consistency oracle

