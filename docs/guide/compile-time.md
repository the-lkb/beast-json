# Compile-Time Impact Guide

qbuem-json is a single-header library that makes heavy use of C++20 concepts,
template metaprogramming, and SIMD intrinsics.  For most projects the compile
overhead is unnoticeable, but large code bases that include
`qbuem_json/qbuem_json.hpp` in many translation units can see measurable build
slowdowns.  This guide explains why, and what you can do about it.

---

## Why the single header is heavy

| Feature | Why it matters for compile time |
|---|---|
| C++20 concepts | Evaluated at every instantiation point |
| `QBUEM_JSON_FIELDS` macro | Expands to five function bodies per annotated struct |
| SIMD intrinsics (SSE4.2 / AVX-512 / NEON) | The compiler must validate intrinsic overloads |
| Schubfach / yy-itoa numeric serializers | ~1,200 lines of constexpr / template code |
| `NexusScanner` + `from_json_direct` | One template instantiation tree per target type |

The total header weighs roughly **8,700 lines**.  In a debug build on a mid-range
workstation, each translation unit that includes it adds about **0.3–0.7 s** of
front-end parse time (measured with `-ftime-report` / `time-trace`).

---

## Strategy 1 — Precompiled headers (recommended)

The fastest fix is a PCH.  Create one file that includes qbuem-json and nothing
else, then compile it once per target.

### CMake (≥ 3.16)

```cmake
target_precompile_headers(my_target PRIVATE
    <qbuem_json/qbuem_json.hpp>
)
```

Downstream targets that link against `my_target` automatically reuse the PCH.

### Manual (GCC / Clang)

```bash
# compile the PCH
g++ -std=c++20 -O2 include/qbuem_json/qbuem_json.hpp -o qbuem_json.hpp.gch

# every TU that includes the header now picks up the .gch automatically
g++ -std=c++20 -O2 -I include my_file.cpp -o my_file.o
```

> **Tip:** PCH files are compiler- and flag-sensitive.  Use the *exact* same
> `-std`, `-O`, and architecture flags as the rest of your build; otherwise the
> compiler silently ignores the PCH and rebuilds from source.

---

## Strategy 2 — Isolation translation unit

Create a single `.cpp` file that includes the header and provides thin wrappers
for the types used by the rest of the project.  All other TUs include only your
wrappers, not the full header.

```cpp
// json_facade.cpp  ← compiled once
#include <qbuem_json/qbuem_json.hpp>

qbuem::Document parse_document(std::string_view s) {
    return qbuem::parse(s);
}

// json_facade.hpp  ← included everywhere else
#pragma once
#include <string_view>
namespace qbuem { struct Document; }
qbuem::Document parse_document(std::string_view s);
```

The drawback is that `qbuem::Value` navigation and `QBUEM_JSON_FIELDS` structs
must be defined in or included from `json_facade.cpp`.  This pattern works best
when JSON handling is concentrated in a few modules.

---

## Strategy 3 — Selective feature control macros

Defining any of the macros below *before* including the header disables or
replaces the corresponding feature, reducing instantiation work:

| Macro | Effect |
|---|---|
| `QBUEM_DISABLE_SIMD` | Disables all SIMD paths; uses portable SWAR fallback |
| `QBUEM_NEXUS_STRICT` | Enables strict type checking in Nexus (adds checks, not templates) |

```cpp
// In a file where you only need DOM navigation, not Nexus:
#define QBUEM_DISABLE_SIMD   // removes SSE/AVX/NEON intrinsic headers
#include <qbuem_json/qbuem_json.hpp>
```

> **Note:** `QBUEM_DISABLE_SIMD` reduces throughput on hot paths.  Only use it
> in files where JSON parsing is infrequent.

---

## Strategy 4 — Unity builds

If your project already uses a unity build (all sources combined into a single
TU), the header is parsed exactly once.  CMake supports this natively:

```cmake
set_target_properties(my_target PROPERTIES UNITY_BUILD ON)
```

---

## Strategy 5 — Measure before optimising

Use `clang -ftime-trace` (Clang ≥ 9) or GCC's `-ftime-report` to identify
which files account for most parse time before applying any of the strategies
above.

```bash
# Clang time-trace — produces a .json per .o, viewable in chrome://tracing
clang++ -std=c++20 -ftime-trace -c my_file.cpp

# GCC time-report — summary to stderr
g++ -std=c++20 -ftime-report -c my_file.cpp 2>&1 | grep -A5 "Time for"
```

Look for the `qbuem_json.hpp` parse and instantiation entries.  If they account
for less than 10 % of total build time, no action is needed.

---

## Practical recommendations for large projects

1. **≤ 20 TUs including the header** — no action required; the default single
   header is fast enough.
2. **20–100 TUs** — add a PCH (`target_precompile_headers`).
3. **> 100 TUs** — combine a PCH with the isolation-TU strategy.  Put all
   `QBUEM_JSON_FIELDS` struct definitions in a shared header that is itself
   covered by the PCH.

---

## Estimating `QBUEM_JSON_FIELDS` cost

Each `QBUEM_JSON_FIELDS(Struct, f1, f2, …)` annotation expands to five inline
function bodies.  For large structs (> 20 fields) in a TU that is included in
many places, the expansion can noticeably slow down the dependent translation
units.

**Mitigation:** Move large `QBUEM_JSON_FIELDS` definitions into a dedicated
`json_types.hpp` that is guarded by the PCH.

```cpp
// json_types.hpp — included by every TU, but covered by the PCH
#pragma once
#include <qbuem_json/qbuem_json.hpp>

struct Order { uint64_t id; std::string symbol; double price; int qty; };
QBUEM_JSON_FIELDS(Order, id, symbol, price, qty)
```

With the PCH in place the instantiation happens once and is shared across the
entire build.
