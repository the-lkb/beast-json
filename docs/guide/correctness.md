# Correctness & Safety

<div style="display: flex; flex-wrap: wrap; gap: 0.4rem; margin: 1rem 0 1.25rem; line-height: 1.9;">
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml/badge.svg" alt="CI" /></a>
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml/badge.svg" alt="Sanitizers (ASan · UBSan · TSan)" /></a>
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml/badge.svg" alt="Benchmark CI" /></a>
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml/badge.svg" alt="CodeQL" /></a>
  <img src="https://img.shields.io/badge/tests-523%20passing-brightgreen" alt="523 tests passing" />
  <img src="https://img.shields.io/badge/fuzz-3%20libFuzzer%20targets-orange" alt="3 libFuzzer targets" />
  <img src="https://img.shields.io/badge/RFC%208259-compliant-brightgreen" alt="RFC 8259" />
  <img src="https://img.shields.io/badge/RFC%206901-JSON%20Pointer-brightgreen" alt="RFC 6901" />
  <img src="https://img.shields.io/badge/RFC%206902-JSON%20Patch-brightgreen" alt="RFC 6902" />
  <img src="https://img.shields.io/badge/IEEE%20754-round--trip-brightgreen" alt="IEEE 754 round-trip" />
</div>

<!-- Summary stats -->
<div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(110px, 1fr)); gap: 0.75rem; background: linear-gradient(135deg, #f0f4ff, #e8f0ff); border: 1px solid #c0d0ff; border-radius: 12px; padding: 1.25rem 1.5rem; margin: 0 0 2rem; text-align: center;">
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">523</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">tests passing</div>
  </div>
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">73</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">RFC 8259 cases</div>
  </div>
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">10</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">CI configurations</div>
  </div>
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">3×</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">sanitizers</div>
  </div>
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">3</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">fuzz targets</div>
  </div>
</div>

This page documents the concrete testing and verification infrastructure behind
qbuem-json.  Every claim here is backed by a CI job you can inspect and
reproduce locally.

---

## At a glance

| Signal | Status | Details |
|:---|:---:|:---|
| Total tests | **523** | 20 test files, 5,556 lines |
| RFC 8259 compliance tests | **73** | y_ accept · n_ reject · i_ implementation-defined |
| RFC 6901 JSON Pointer | ✅ | Pointer navigation + edge cases |
| RFC 6902 JSON Patch | ✅ | add / remove / replace / move / copy / test ops, transactional rollback |
| AddressSanitizer (ASan) | ✅ CI | [sanitizers.yml](https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml) |
| UndefinedBehaviorSanitizer (UBSan) | ✅ CI | [sanitizers.yml](https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml) |
| ThreadSanitizer (TSan) | ✅ CI | [sanitizers.yml](https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml) |
| Fuzz testing | ✅ | 3 libFuzzer targets · seed corpus |
| IEEE 754 round-trip | ✅ | All 64-bit doubles; Schubfach serialization (Giulietti 2020) + `std::strtod` parsing |
| CodeQL static analysis | ✅ CI weekly | security-extended query suite |
| Multi-platform CI | ✅ [10 configs](https://github.com/qbuem/qbuem-json/blob/main/.github/workflows/ci.yml) | GCC 13/14 · Clang 18 · Apple Clang · x86_64 · aarch64 · Apple Silicon |

---

## RFC 8259 compliance

qbuem-json ships 73 RFC 8259 test cases in `tests/test_compliance.cpp` following
the [JSONTestSuite](https://github.com/nst/JSONTestSuite) naming convention:

| Prefix | Meaning | Count |
|:---|:---|---:|
| `y_` | **Must accept** — valid JSON; parser must not throw | 45 |
| `n_` | **Must reject** — invalid JSON; `parse_strict()` must throw | 22 |
| `i_` | **Implementation-defined** — we document our choice | 6 |

### What is tested

- All JSON value types at the root level (`null`, `true`, `false`, numbers,
  strings, arrays, objects)
- Number forms: zero, negative zero, integers, floats, exponents, edge-case
  exponents (`1.23e+456`)
- String escapes: `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`,
  `\uXXXX`, surrogate pairs
- Structural constraints: empty object, empty array, deeply nested structures,
  trailing commas (rejected in strict mode)
- RFC 6901 JSON Pointer: simple paths, `/`, `~0`/`~1` escapes, array indexing,
  end-of-array token `-`, leading-zero rejection
- RFC 6902 JSON Patch: all six operations plus transactional rollback (a
  mid-patch failure must leave the document unchanged)

### Lenient vs strict mode

The library ships two parsers:

| Function | Mode | Behaviour |
|:---|:---|:---|
| `qbuem::parse()` | Lenient | Accepts relaxed JSON (comments, trailing commas) |
| `qbuem::parse_strict()` | RFC 8259 strict | Rejects anything outside the spec |

All `n_` tests use `parse_strict()`.  The lenient parser's extensions are
explicitly documented so you can opt in intentionally.

---

## Memory safety — sanitizers

Sanitizer CI runs on every push and pull request to `main` when `include/`,
`tests/`, or `CMakeLists.txt` changes.

| Job | Tool | What it catches |
|:---|:---|:---|
| `asan-ubsan` | Clang 18 ASan + UBSan | Heap/stack overflows · use-after-free · use-after-scope · double-free · memory leaks · signed integer overflow · null pointer dereference · misaligned access · out-of-bounds array indexing |
| `tsan` | Clang 18 TSan | Data races · lock-order inversions · use of uninitialised mutexes |

> ASan and TSan are incompatible and run in separate jobs, as required by the
> LLVM toolchain.

**Run locally:**

```bash
# ASan + UBSan
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DQBUEM_JSON_BUILD_TESTS=ON \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build -j$(nproc)
ASAN_OPTIONS="halt_on_error=1:detect_leaks=1" \
UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
ctest --test-dir build --output-on-failure

# TSan
cmake -B build_tsan -DCMAKE_BUILD_TYPE=Debug \
  -DQBUEM_JSON_BUILD_TESTS=ON \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer"
cmake --build build_tsan -j$(nproc)
TSAN_OPTIONS="halt_on_error=1" \
ctest --test-dir build_tsan --output-on-failure
```

---

## Fuzz testing

Three [libFuzzer](https://llvm.org/docs/LibFuzzer.html) targets are maintained
in `fuzz/`:

| Target | Input | What it tests |
|:---|:---|:---|
| `fuzz_dom` | Arbitrary bytes | DOM parser — crash/hang on malformed input |
| `fuzz_parse` | Arbitrary bytes | Nexus/struct-mapping path — type safety under adversarial data |
| `fuzz_rfc8259` | Arbitrary bytes | Strict parser — RFC 8259 acceptance/rejection consistency |

A seed corpus in `fuzz/corpus/` seeds each target with valid JSON samples from
the benchmark suite (twitter.json, canada.json, citm.json, gsoc.json).

**Build and run a fuzz target:**

```bash
cmake -B build_fuzz \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DQBUEM_JSON_BUILD_FUZZ=ON \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,fuzzer"
cmake --build build_fuzz -j$(nproc)

# Run fuzz_dom for 60 seconds with the seed corpus:
./build_fuzz/fuzz/fuzz_dom fuzz/corpus -max_total_time=60
```

---

## IEEE 754 floating-point correctness

Every `double` parsed by qbuem-json and every `double` serialised by qbuem-json
satisfies the **shortest round-trip** guarantee:

> `parse(serialize(x)) == x` for all finite `double` values.

This is enforced by two algorithms working in tandem:

- **Parsing** — `std::strtod` (C standard library).  Correct for all IEEE 754
  finite doubles when fed the shortest-decimal output of Schubfach.
- **Serialisation** — Schubfach (Giulietti 2020) produces the unique shortest
  decimal representation.  No trailing zeros, no round-trip loss.

Test coverage:

```cpp
// tests/test_serializer.cpp — excerpt
TEST(Roundtrip, AllSpecialDoubles) {
    for (double v : {0.0, -0.0, 1.0, -1.0, DBL_MIN, DBL_MAX,
                     1.0/3.0, M_PI, 1e300, 5e-324}) {
        auto s = qbuem::write(v);
        EXPECT_EQ(qbuem::read<double>(s), v);
    }
}
```

---

## Test suite breakdown

| File | Tests | What it covers |
|:---|---:|:---|
| `test_value_accessors.cpp` | ~200 | DOM `Value` access: `as<T>()`, `try_as<T>()`, `is_*()`, `operator[]`, `at()` |
| `test_stl_exhaustive.cpp` | ~120 | All STL container types via Nexus: vector, map, set, list, deque, optional, variant, tuple, pair |
| `test_complex_stl.cpp` | ~80 | Deeply nested STL combinations (`map<string, vector<optional<int>>>` etc.) |
| `test_compliance.cpp` | **73** | RFC 8259, RFC 6901, RFC 6902 — canonical spec conformance |
| `test_dom_roundtrip.cpp` | ~20 | Parse → mutate → re-serialise equality |
| `test_mutations.cpp` | ~25 | In-place scalar and structural mutations |
| `test_unicode.cpp` | ~15 | UTF-8 validation, `\uXXXX` escape decode, surrogate pairs |
| `test_serializer.cpp` | ~20 | Schubfach floats, yy-itoa integers, edge cases (NaN, Inf, DBL_MAX) |
| `test_errors.cpp` | ~15 | Parse error propagation, type error messages, lifetime errors |
| `repro_bugs.cpp` | ~40 | Regression tests for every previously-reported bug |
| `test_control_char.cpp` | ~10 | Bare control characters in strings (strict reject, lenient accept) |
| `test_utf8_validation.cpp` | ~10 | Malformed UTF-8 sequences |
| `test_trailing_commas.cpp` | ~8 | Trailing comma behaviour in strict vs lenient mode |
| `test_duplicate_keys.cpp` | ~8 | Duplicate key last-write-wins semantics |
| `test_comments.cpp` | ~8 | `//` and `/* */` comment handling in lenient mode |
| `test_relaxed.cpp` | ~8 | Lenient mode acceptance of non-standard extensions |
| `test_bitmap.cpp` | ~6 | SIMD bitmask correctness for structural characters |
| `test_bitmap_offsets.cpp` | ~6 | Bitmask byte-offset alignment edge cases |
| `test_dom_types.cpp` | ~10 | DOM type tag correctness |

---

## Running the full test suite yourself

```bash
git clone https://github.com/qbuem/qbuem-json.git
cd qbuem-json

# Standard build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DQBUEM_JSON_BUILD_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

# Expected output:
# 523/523 Test #523: StringEdge.SpecialKeyNames .......... Passed  0.00 sec
# 100% tests passed, 0 tests failed out of 523
```

The test suite takes under 1 second on any modern machine.

---

## CodeQL static analysis

A [CodeQL workflow](https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml)
runs the `security-extended` query suite on every push to `main` and on a
weekly schedule.  This catches:

- Buffer overruns, integer overflows, uncontrolled format strings
- Use of unsafe C standard library functions
- Injection-related patterns (not directly applicable to a JSON library, but
  caught proactively)

Results are visible in the
[GitHub Security tab](https://github.com/qbuem/qbuem-json/security/code-scanning).
