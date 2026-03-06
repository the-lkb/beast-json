# Security — Memory-Safety Hardening

> **Summary**: 5 memory-safety vulnerabilities were discovered through AddressSanitizer (ASan),
> UndefinedBehaviorSanitizer (UBSan), and libFuzzer-guided fuzzing, then fixed.
> All fixes are defence-in-depth guards on malformed / adversarial input;
> they have **no measurable performance impact** on well-formed JSON
> (verified by before/after benchmarks — see below).

---

## Vulnerability Report

### Bug 1 — Null-Dereference in `skip_to_action()` *(heap-buffer-overflow)*

| Field | Detail |
|-------|--------|
| **Trigger** | Input `[` (unterminated array, 1 byte) |
| **Root cause** | `skip_to_action()` dereferenced `*p_` without checking `p_ < end_` first. Any input whose last meaningful byte is `[` or `{` causes the parser to re-enter `skip_to_action()` with `p_ == end_`. |
| **Impact** | 1-byte read past the JSON allocation boundary |
| **Fix** | Added `if (BEAST_UNLIKELY(p_ >= end_)) return 0;` as the very first statement in `skip_to_action()`. |

---

### Bug 2 — Empty-Tape Read via Bare Separator *(heap-buffer-overflow)*

| Field | Detail |
|-------|--------|
| **Trigger** | Input `,` (single comma, 1 byte) |
| **Root cause** | `kActComma` / `kActColon` consumed bytes without writing any tape node. `depth_ == 0` caused `parse()` / `parse_staged()` to return `true` with an empty tape. Subsequent `tape[0]` access read from uninitialized `malloc` memory. |
| **Impact** | Heap-buffer-overflow on the first tape read |
| **Fix** | The `done:` label return condition in both `parse()` and `parse_staged()` now requires `tape_head_ > doc_->tape.base` in addition to `depth_ == 0`. |

---

### Bug 3 — Non-String Object Keys + Iterator Out-of-Bounds *(heap-buffer-overflow)*

| Field | Detail |
|-------|--------|
| **Trigger** | Inputs `{false}`, `{]\x01...` |
| **Root cause (3a)** | The parser's `kActTrue`, `kActFalse`, `kActNull`, `kActNumber`, `kActObjOpen`, `kActArrOpen` cases lacked an `is_key` state check. RFC 8259 §4 requires object keys to be strings only; accepting other types produces structurally ambiguous tape layouts. |
| **Root cause (3b)** | `skip_val_s_()`, `ObjectIterator::skip_deleted_()`, and `ArrayIterator::skip_deleted_()` accessed `doc_->tape[i]` without a tape-size bounds check, allowing reads past `tape.head` on malformed tape. |
| **Impact** | Heap-buffer-overflow during object iteration on malformed tapes |
| **Fix** | Added `if (BEAST_UNLIKELY(cur_state_ & 0b001u)) goto fail;` to all six non-string value cases. Added `tape_sz` bounds guard at the top of both `skip_deleted_()` methods and in `skip_val_s_()`. |

---

### Bug 4 — Stale Overlay Maps + Stack Underflow in `dump_changes_()` *(UBSan index -1 / stack-buffer-underflow)*

| Field | Detail |
|-------|--------|
| **Trigger** | `[\x03\x00:}` fed to `fuzz_lazy` after a prior call that performed `insert`/`erase` mutations |
| **Root cause (4a)** | `parse_reuse()` did not clear `mutations_`, `deleted_`, or `additions_` between calls. Stale overlay entries referenced tape indices from a prior parse; on the next call those indices no longer meant the same thing, corrupting `dump_changes_()` output. |
| **Root cause (4b)** | `dump_changes_()` accessed `stk[top]` when `top == -1`, hitting an `ObjectEnd` / `ArrayEnd` node with no matching open node on the stack. |
| **Impact** | UBSan "index -1 out of bounds" + 8-byte stack-buffer-underflow in `dump_changes_()` |
| **Fix (4a)** | Added `.clear()` for all three overlay maps at the start of `parse_reuse()`. |
| **Fix (4b)** | Added `if (BEAST_UNLIKELY(top < 0))` early-exit guards for the `ObjectEnd` and `ArrayEnd` cases in `dump_changes_()`. |

---

### Bug 5 — `skip_value_()` Out-of-Bounds + `memcpy` Past Source End *(heap-buffer-overflow)*

| Field | Detail |
|-------|--------|
| **Trigger** | Multi-invocation sequence with static `g_doc`; intermediate failed parses leave stale tape content beyond `tape.head` |
| **Root cause (5a)** | `skip_value_()` depth-tracking loop accessed `doc_->tape[idx]` with no bounds check. If the tape contained an `ObjectStart` without a matching `ObjectEnd` within `tape.size()`, the loop continued reading past `tape.cap` into adjacent heap memory. |
| **Root cause (5b)** | `dump_subtree_()` and `dump_changes_()` used `nd.offset + len` as a source read extent without verifying it fit within `doc_->source.size()`. On malformed tape produced by a partial/failed parse, `nd.offset + len` could exceed the source allocation. |
| **Impact** | Heap-buffer-overflow in `TapeNode::type()` (reads past tape allocation) and in `memcpy` (reads past source allocation) |
| **Fix (5a)** | `skip_value_()` now checks `idx >= tsz` before the initial access, and the inner loop condition is `depth > 0 && idx < tsz`. |
| **Fix (5b)** | `dump_subtree_()` and `dump_changes_()` now clamp string/number copy lengths: `std::min(len, source.size() - nd.offset)`. The main `dump()` hot loop (root value, `idx_ == 0`) is unaffected. |

---

## Performance Impact

Benchmarked Before/After on Linux x86-64, GCC 13, `-O3 -flto -march=native`,
300 iterations each, 3 independent runs, median reported.

### Parse (μs) — lower is better

| File | Before | After | Δ |
|------|-------:|------:|---|
| twitter.json (616 KB) | 259 | 266 | +2.7% ↔ noise |
| canada.json (2.2 MB) | 1,585 | 1,585 | **0.0%** |
| citm_catalog.json (1.7 MB) | 632 | 644 | +1.9% ↔ noise |
| gsoc-2018.json (3.2 MB) | 781 | 792 | +1.4% ↔ noise |

### Serialize (μs) — lower is better

| File | Before | After | Δ |
|------|-------:|------:|---|
| twitter.json | 159 | 161 | +1.3% ↔ noise |
| canada.json | 1,047 | 1,045 | **-0.2%** |
| citm_catalog.json | 432 | 423 | **-2.1%** |
| gsoc-2018.json | 353 | 366 | +3.7% ↔ noise |

> All deltas are within the ±5% measurement noise band of this environment.
> The main `dump()` hot loop and the main `parse()` hot loop are structurally
> unchanged — all fixes touch only `BEAST_UNLIKELY` branches or paths that are
> not reachable with well-formed JSON.

---

## Fuzz Infrastructure

Three libFuzzer targets (Clang 18, static ASan+UBSan, `-fsanitize=address,undefined`):

| Target | Coverage |
|--------|----------|
| `fuzz/fuzz_parse.cpp` | `beast::parse()`, all type accessors, `dump()`, `dump(indent)`, object/array iteration, SafeValue chain, JSON Pointer, pipe fallback |
| `fuzz/fuzz_lazy.cpp` | `parse_reuse()`, same accessors as above, plus `insert` / `erase` / `push_back` mutations, `type_name()` |
| `fuzz/fuzz_rfc8259.cpp` | Consistency oracle: parses the same input into two independent `DocumentView` instances and `__builtin_trap()`s if `dump()` outputs diverge |

### Building the fuzz targets

```bash
# Requires Clang 18 + libclang-rt-18-dev
cmake -S . -B build-fuzz \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBEAST_JSON_BUILD_FUZZ=ON \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=fuzzer,address,undefined"
cmake --build build-fuzz -j$(nproc)
```

### Running

```bash
mkdir -p fuzz/corpus
# Run each target for 60+ seconds
./build-fuzz/fuzz/fuzz_parse  fuzz/corpus/ -max_total_time=60
./build-fuzz/fuzz/fuzz_lazy   fuzz/corpus/ -max_total_time=60
./build-fuzz/fuzz/fuzz_rfc8259 fuzz/corpus/ -max_total_time=60
```

### Crash reproduction files

All 10 crash inputs discovered during fuzzing are stored in `crash-*/` at the
repository root. After the fixes, every file produces a clean exit (0) across
all three targets.

---

## Reporting New Vulnerabilities

Please open an issue on the repository with the label **security** and include:
- A minimal reproducer (the crash input file or a description)
- The sanitizer error output
- The library version / commit hash
