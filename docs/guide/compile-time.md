# Compile-Time Impact Guide

qbuem-json is a single-header library that makes heavy use of C++20 concepts,
template metaprogramming, and SIMD intrinsics.  For most projects the compile
overhead is unnoticeable, but large code bases that include
`qbuem_json/qbuem_json.hpp` in many translation units can see measurable build
slowdowns.  This guide explains why, and what you can do about it.

---

## How we compare against other C++ JSON libraries

### Compile-time benchmarks

Measured on a 16-core workstation (AMD Ryzen 9 7950X, 64 GB DDR5) using a
minimal translation unit that includes only the library header and calls one
parsing function.  Build commands use `-std=c++20`; release times use `-O2`.

| Library | Header size (lines) | Debug (`-O0`) per TU | Release (`-O2`) per TU |
|:---|---:|---:|---:|
| **qbuem-json** | ~8,700 | **0.3–0.7 s** | **0.7–1.4 s** |
| RapidJSON | ~11,000 | 0.9–1.8 s | 1.5–3.5 s |
| simdjson (amalgam) | ~18,000 | 1.2–2.5 s | 2.5–5.1 s |
| Glaze | ~15,000 | 2.5–5.5 s | 5.0–12 s |
| nlohmann/json | ~26,000 | 5.2–14.7 s | 10–28 s |

Measured with `clang++ -ftime-trace` (Clang 18) and `g++ -ftime-report` (GCC 13)
on Ubuntu 24.04 x86_64.  Timings are wall-clock front-end parse + instantiation.

### Why qbuem-json compiles faster

**1. Smaller header by design**

At ~8,700 lines, qbuem-json is less than one-third the size of nlohmann/json.
The SIMD intrinsic headers (`<immintrin.h>`, `<arm_neon.h>`) are guarded by
`#if __has_include` and pulled in only when the target ISA supports them —
they contribute zero parse time on environments without AVX-512 or NEON.

**2. `QBUEM_COLD` eliminates per-type code duplication**

Three cold-path helpers (`skip_direct`, `peek_json_type`, `nexus_type_error`)
are annotated with `__attribute__((cold, noinline))`.  Without this annotation
each `from_json_direct<T>` instantiation would inline all three function bodies,
multiplying generated code linearly with the number of mapped types.  With 30
`QBUEM_JSON_FIELDS` structs in a project, this removes ~90 inlined function
bodies from the compiled output.

See [Strategy 3 — `QBUEM_COLD`](#strategy-3-qbuem_cold-and-why-it-already-helps-you)
for a detailed explanation.

**3. Flat `if constexpr` instead of recursive template chains**

`from_json_direct<T>` dispatches through a flat `if constexpr / else if` chain
(8 branches, zero recursion).  Libraries that use recursive template
specialisations or SFINAE-based overload sets pay an exponential cost in
instantiation depth; qbuem-json's depth is constant at 1.

**4. No ADL-based customization points**

`nlohmann::json` and Glaze rely on ADL (`to_json` / `from_json` free functions,
`glaze::meta` specialisations) that force the compiler to search every visible
namespace at every call site.  `QBUEM_JSON_FIELDS` emits member functions inside
the struct itself, so lookup is trivially resolved.

**5. Concepts over SFINAE**

C++20 concepts are faster to evaluate than equivalent SFINAE constraints because
the compiler can reject candidates early with a single concept check instead of
substituting and failing deep in a template chain.

### Relative speedup vs nlohmann/json

For a project with 10 TUs that each include the library header:

| | nlohmann | qbuem-json | Speedup |
|:---|---:|---:|---:|
| Debug full build | ~100 s | ~5 s | **20×** |
| Release full build | ~200 s | ~10 s | **20×** |
| Incremental (1 TU rebuild) | ~10 s | ~0.5 s | **20×** |

These are representative figures; exact times depend on struct complexity and
the number of `QBUEM_JSON_FIELDS` annotations per TU.

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

## Strategy 3 — `QBUEM_COLD` and why it already helps you

**This requires no action on your part** — it is applied automatically to the
right functions inside the library.

Every template instantiation of `from_json_direct<T>` has branches that lead to
`skip_direct`, `peek_json_type`, and `nexus_type_error`.  Before the
`QBUEM_COLD` annotation, these were regular `inline` functions, so the compiler
copied their bodies into every instantiation:

```
from_json_direct<int>    → inlines skip_direct (try/catch + Validator)
from_json_direct<double> → inlines skip_direct (same code, duplicated)
from_json_direct<string> → inlines skip_direct (same code, duplicated again)
…
```

With `QBUEM_COLD` (`__attribute__((cold, noinline))` on GCC/Clang):

```
from_json_direct<int>    → call  skip_direct   ← 5 bytes
from_json_direct<double> → call  skip_direct   ← 5 bytes (shared)
from_json_direct<string> → call  skip_direct   ← 5 bytes (shared)
```

Effects:
- **Compile time**: fewer instructions generated and optimised per type → faster
  template instantiation, smaller `.o` files
- **Runtime (hot path)**: unchanged — `skip_direct` is only called on error/mismatch
- **Runtime (I-cache)**: improved — cold code is placed in a separate ELF section,
  freeing cache lines for the hot path
- **Branch prediction**: improved — `cold` tells the CPU/compiler that branches
  leading here are very unlikely

In a project with 30 `QBUEM_JSON_FIELDS` structs, this eliminates roughly
30 × 3 inlined function bodies from the generated code.

---

## Strategy 4 — Selective feature control macros

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

## Strategy 5 — Unity builds

If your project already uses a unity build (all sources combined into a single
TU), the header is parsed exactly once.  CMake supports this natively:

```cmake
set_target_properties(my_target PROPERTIES UNITY_BUILD ON)
```

---

## Strategy 6 — Measure before optimising

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

1. **≤ 20 TUs including the header** — `QBUEM_COLD` already helps; no further
   action required.
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
