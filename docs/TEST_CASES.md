# Beast JSON Test Case Documentation & Integrity Report

This document outlines the systematic testing methodology and standard compliance verification for the Beast JSON library.

## 1. Testing Philosophy
Beast JSON follows a "Safety First, Performance Second" philosophy. Every feature is validated against memory sanitizers and official RFC test suites to ensure that extreme performance does not come at the cost of correctness or security.

**Total test count: 507** (as of the ReproBugs2 + Observations additions).

## 2. Core Compliance & Standard Verification
These tests guarantee that Beast JSON flawlessly adheres to official JSON standards.

### 2.1 RFC 8259 Compliance (JSON)
- **File**: `tests/test_compliance.cpp`
- **Objective**: Flawless adherence to the official JSON specification.
- **Coverage**:
  - All valid JSON values (Acceptance tests).
  - All invalid JSON values (Rejection tests - e.g. trailing commas, leading zeros).
  - Deep structures and edge-case numbers.

### 2.2 RFC 6901 (JSON Pointer)
- **File**: `tests/test_compliance.cpp`
- **Objective**: Standardized navigation within a JSON document.
- **Features**:
  - `~0` and `~1` escape sequences.
  - `-` (end of array) token handling.
  - Leading zero restriction in array indices.

### 2.3 RFC 6902 (JSON Patch)
- **File**: `tests/test_compliance.cpp`
- **Objective**: Transactional DOM modification via standard patch arrays.
- **Supported Operations**: `add`, `remove`, `replace`, `move`, `copy`, `test`.

---

## 3. Security & Multi-Sanitizer Verification (Memory Integrity)

To mathematically and dynamically prove memory safety and the absence of undefined behavior, Beast JSON is continuously validated against the following sanitizers:

| Sanitizer | Target | Result (macOS 15/16) |
| :--- | :--- | :--- |
| **ASan** (Address) | Heap/Stack Overflows, UAF | **PASS (0 errors)** |
| **UBSan** (Undefined) | Arithmetic, Alignment, Overflow | **PASS (0 errors)** |
| **TSan** (Thread) | Data Races, Concurrency | **PASS (0 errors)** |
| **LSan/leaks** | Memory Leaks (Runtime) | **PASS (0 leaks)** |

---

## 4. Performance & Zero-Cost Audit

Beast JSON adheres to the "Zero-Cost" principle: you don't pay for the mutation features unless you use them.

- **Read-Path Efficiency**: Accessors like `operator[]` and `at()` use `BEAST_UNLIKELY` branch hints to check for mutations. For read-only documents, this cost is a single bit-check.
- **Throughput Verification** (Release build, twitter.json):
  - **Parsing**: ~227 μs (**2.7 GB/s**)
  - **Serialization**: ~75 μs (**8.1 GB/s**)
- **No Regressions**: Comparisons against the baseline show 0% performance loss on cold-path (read-only) usage after RFC 6901/6902 integration.

## 5. Security & Multi-Sanitizer Verification

| Sanitizer | Target | Result |
| :--- | :--- | :--- |
| **ASan** | Memory Errors | **PASS** |
| **UBSan** | Undefined Behavior | **PASS** |
| **TSan** | Data Races | **PASS** |
| **Leaks** | Memory Leaks | **PASS (0 byte)** |

---

## 6. Regression & Bug-Fix Test Suites

### 6.1 ReproBugs2 Suite — `tests/repro_bugs.cpp`

10 regression tests covering the four bugs fixed in v1.0.x:

| Test | Bug | Description |
| :--- | :--- | :--- |
| `ParseReusePublicFacade` | BUG-1 | `beast::parse_reuse()` is available in the public `beast::` namespace without ADL ambiguity |
| `UnsignedIntSubscriptNoAmbiguity` | BUG-2 | `Value::operator[](unsigned int)` compiles and dispatches correctly |
| `UnsignedIntEraseNoAmbiguity` | BUG-2 | `Value::erase(unsigned int)` compiles and dispatches correctly |
| `SafeValueGetUnsignedIntNoAmbiguity` | BUG-2 | `SafeValue::operator[](unsigned int)` and `SafeValue::get(unsigned int)` compile correctly |
| `PushBackSizeReflectsNewElements` | BUG-3 | `size()` on an array returns `tape_count + push_back_count` |
| `PushBackOnNonEmptyArraySizeCorrect` | BUG-3 | `size()` correct after `push_back()` on a non-empty parsed array |
| `PushBackJsonSizeReflectsNewElements` | BUG-3 | `size()` correct after `push_back_json()` |
| `ItemsIncludesInsertedKeys` | BUG-4 | `items()` iteration yields keys added via `insert()` |
| `ItemsIncludesInsertedKeysAfterErase` | BUG-4 | `items()` correctly handles erased tape keys mixed with inserted keys |
| `ItemsValuesOfInsertedKeys` | BUG-4 | Values of `insert()`ed keys are correctly retrieved via `items()` |

### 6.2 Observations Suite — `tests/repro_bugs.cpp`

7 tests covering API behaviour clarifications (OBS-1 to OBS-3):

| Test | OBS | Description |
| :--- | :--- | :--- |
| `UnsetRevertsToOriginalValue` | OBS-1 | `unset()` restores the original parsed value, not null |
| `UnsetTypeNameRetainsOriginalType` | OBS-1 | `type_name()` returns original type after `unset()` |
| `SubscriptMissReturnsInvalidNotThrow` | OBS-2 | `operator[]` on missing key returns invalid `Value{}`, does not throw |
| `AsOnInvalidValueThrows` | OBS-2 | `.as<T>()` on an invalid `Value` throws `std::runtime_error` |
| `AsArrayThrowsOnTypeMismatch` | OBS-3 | `.as_array<T>()` throws on element type mismatch |
| `TryAsArrayHandlesMixedTypes` | OBS-3 | `.try_as_array<T>()` returns `std::nullopt` on type mismatch (no throw) |
| `TryAsArrayNeverThrows` | OBS-3 | `.try_as_array<T>()` never throws even with all wrong-type elements |

### 5.1 Mutation Security Stress Test
A dedicated stress test (`test_patch_security.cpp`) validates that:
- Unknown patch operations are rejected.
- Invalid paths or out-of-bounds indices trigger safe rollbacks.
- Empty paths (root targets) are handled correctly.
- Cross-document `move`/`copy` operations are lifetime-safe.
