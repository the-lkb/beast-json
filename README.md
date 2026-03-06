# Beast JSON

**The fastest C++20 JSON library you can drop into any project — single header, zero dependencies.**

Beast JSON is a high-performance, header-only C++ JSON parser and serializer built on a tape-based lazy DOM. It beats [yyjson](https://github.com/ibireme/yyjson) — currently the world's fastest JSON library — on parse throughput across all standard benchmark files on Linux x86_64 and Android AArch64, while offering a developer experience on par with [nlohmann/json](https://github.com/nlohmann/json).

> **An AI-Only Generated Library** — every line of code, every optimization, and every benchmark in this repository was designed and written by AI (Claude/Gemini). An ongoing experiment in what AI can achieve in low-level, high-performance C++ systems programming.

---

## Performance at a Glance

> Linux x86_64, GCC 13.3, `-O3 -flto -march=native` + parse-only PGO, vs yyjson with full SIMD.
> Full benchmark tables for all 3 platforms: **[docs/PERFORMANCE.md](docs/PERFORMANCE.md)**

| File | Beast parse | yyjson parse | Ratio | Beast serialize | yyjson serialize |
|:---|---:|---:|:---:|---:|---:|
| twitter.json (617 KB) | **189 μs** · 3.27 GB/s | 282 μs | **1.49×** ✅ | 145 μs | 131 μs |
| canada.json (2.2 MB) | **1,433 μs** · 1.54 GB/s | 2,595 μs | **1.81×** ✅ | **789 μs** | 3,301 μs |
| citm_catalog.json (1.7 MB) | **626 μs** · 2.70 GB/s | 757 μs | **1.21×** ✅ | 312 μs | 235 μs |
| gsoc-2018.json (3.2 MB) | **731 μs** · 4.45 GB/s | 1,615 μs | **2.21×** ✅ | **369 μs** | 1,417 μs |

**Beats yyjson ≥1.2× on parse for all 4 files** (Phase 75). Canada and gsoc serialize are **4× faster** than yyjson. Competitive with [simdjson](https://github.com/simdjson/simdjson) and **23× faster** than nlohmann on twitter.json.

### Multi-Platform Status

| Platform | Parse vs yyjson | Serialize vs yyjson | Milestone |
|:---|:---:|:---:|:---|
| **Linux x86_64** (GCC 13.3, AVX-512, PGO) | **+21% to +121%** ✅ | Mixed (2/4 >1.2×) | Phase 75 |
| **Snapdragon 8 Gen 2** (Android, Clang 21) | **+67% to +153%** ✅ | **2.2× to 5.6×** ✅ | Phase 73 · 8/8 swept |
| **Apple M1 Pro** (Apple Clang, NEON, PGO) | Mixed (1/4) | **tied to 4.2×** ✅ | Phase 80-M1 |

---

## Quick Start

```bash
# Single header — no CMake needed
cp include/beast_json/beast_json.hpp /your/project/
```

```cpp
#include "beast_json.hpp"
#include <iostream>

int main() {
    beast::Document doc;
    auto root = beast::parse(doc, R"({
        "name": "Alice",
        "age":  30,
        "tags": ["admin", "user"]
    })");

    // Implicit conversion — nlohmann-style
    std::string name = root["name"];   // "Alice"
    int age          = root["age"];    // 30

    // Type-checked access
    std::string name2 = root["name"].as<std::string>();

    // Pipe fallback — never throws, returns default on miss
    int timeout = root["config"]["timeout"] | 5000;

    // Iteration
    for (auto tag : root["tags"].elements())
        std::cout << tag.as<std::string>() << "\n";

    // Structural mutation (non-destructive overlay)
    root["tags"].push_back(std::string_view{"moderator"});
    root.insert("active", true);
    root["tags"].erase(0);  // remove "admin"

    std::cout << root.dump(2) << "\n";  // pretty-print
}
```

```bash
# Or with CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBEAST_JSON_BUILD_TESTS=ON
cmake --build build && ctest --test-dir build  # 368/368 PASS
```

---

## Why Beast JSON

### Speed first

| Technique | Impact |
|:---|:---|
| **8-byte TapeNode** — `type(8) \| sep(8) \| length(16) \| offset(32)` | 33% smaller working set vs 12-byte predecessor; L2/L3 cache–friendly |
| **Two-phase AVX-512 parsing** — Stage 1 structural index + Stage 2 tape build | twitter.json: 365 → 202 μs (−44.7%) |
| **SWAR string scanning** — 8 bytes/cycle via 64-bit GPR bitwise tricks, no SIMD | Architecture-portable; ~36% of strings exit in first 8-byte chunk |
| **Pre-flagged separators** — `,`/`:` baked into tape at parse time, never recomputed | Serialize hot loop: `if (sep) *w++ = sep==2 ? ':' : ','` — 2 instructions |
| **KeyLenCache** — 264-byte schema-prediction cache, O(1) key scan after first object | citm_catalog: 2,187 key scans → single-byte compare each |
| **Buffer-reuse serialize** — `dump(string&)` + `last_dump_size_` cache | Eliminates malloc+zero-fill on repeated calls; Snapdragon citm: −71% |

### Developer experience on par with nlohmann

```cpp
// ── Access ─────────────────────────────────────────────────────────────
int id   = root["user"]["id"];               // implicit conversion
double v = root["score"] | 0.0;             // pipe fallback, never throws
auto s   = root.get("cfg")["timeout"]        // SafeValue monad chain
               .value_or(5000);

// ── Mutation ────────────────────────────────────────────────────────────
root["name"] = "Bob";                        // operator= (scalar overlay)
root.insert("version", 2);                  // add key
root["tags"].push_back(std::string_view{"x"}); // append array element
root["tags"].erase(0);                       // remove by index

// ── Iteration ───────────────────────────────────────────────────────────
for (auto [key, val] : root.items())         // structured bindings
    std::cout << key << ": " << val.dump() << "\n";

for (int id : root["ids"].as_array<int>())   // typed lazy view
    std::cout << id << " ";

// ── C++20 Ranges ────────────────────────────────────────────────────────
auto big = root["scores"].elements()
    | std::views::filter([](auto v){ return v.as<int>() > 3; });

// ── JSON Pointer ────────────────────────────────────────────────────────
auto v1 = root.at("/user/addr/city");        // RFC 6901 runtime
auto v2 = root.at<"/user/addr/city">();      // compile-time validated

// ── RFC 7396 Merge Patch ─────────────────────────────────────────────────
root.merge_patch(R"({"timeout":null,"debug":true})");
```

### Auto-serialization with zero boilerplate

```cpp
struct User {
    std::string              name;
    int                      age   = 0;
    std::vector<std::string> tags;
    std::optional<double>    score;
};
BEAST_JSON_FIELDS(User, name, age, tags, score)  // ← one line

// Round-trip
auto user = beast::read<User>(json_string);
std::string out = beast::write(user);

// All STL types work without any macro
auto v = beast::read<std::vector<int>>("[1,2,3]");
auto m = beast::read<std::map<std::string,double>>(R"({"pi":3.14})");
auto t = beast::read<std::tuple<int,std::string,bool>>("[42,\"ok\",true]");
```

---

## Feature Overview

| Category | Feature |
|:---|:---|
| **Parsing** | Single header · Zero dependencies · C++17/20 · CMake 3.14+ |
| **Performance** | Tape-based lazy DOM · AVX-512 two-phase parsing · SWAR string scan · KeyLenCache |
| **Access** | `as<T>()` · `try_as<T>()` · implicit `operator T()` · `operator\|` pipe fallback |
| **Safety** | `SafeValue` monad chain · non-throwing `operator[]` · `find()` → `optional<Value>` |
| **Mutation** | Scalar overlay (`set`/`unset`) · structural overlay (`insert`/`erase`/`push_back`) |
| **Iteration** | `items()` · `elements()` · `keys()` · `values()` · `as_array<T>()` typed view |
| **C++20** | `std::ranges::borrowed_range` · `std::views::filter/transform` · Concepts · NTTP JSON Pointer |
| **Serialization** | `dump()` · `dump(string&)` buffer-reuse · `dump(indent)` pretty-print · subtree dump |
| **Standards** | RFC 8259 strict validator · RFC 6901 JSON Pointer · RFC 7396 Merge Patch |
| **Auto-serial** | `beast::read<T>()` · `beast::write()` · `BEAST_JSON_FIELDS` macro · full STL support |
| **Bindings** | C shared library (`beast_json_c.so`) · Python ctypes (`from beast_json import Document`) |
| **Quality** | 368 tests · ASan/UBSan hardened · libFuzzer corpus · 5 CVEs fixed |

---

## Installation

### Single header (simplest)

```cpp
// Copy include/beast_json/beast_json.hpp — no CMake, no dependencies
#include "beast_json.hpp"
```

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(beast_json
    GIT_REPOSITORY https://github.com/kyubuem/beast-json
    GIT_TAG        main)
FetchContent_MakeAvailable(beast_json)
target_link_libraries(your_target PRIVATE beast_json)
```

### Clone & build

```bash
git clone https://github.com/kyubuem/beast-json.git
cd beast-json
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBEAST_JSON_BUILD_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build  # 368/368 PASS
```

---

## RFC 8259 Strict Mode

```cpp
// Standalone validator — zero allocation
try { beast::rfc8259::validate("[1,2,]"); }
catch (const std::runtime_error& e) { /* offset-annotated message */ }

// Validate + parse in one call
beast::Document doc;
auto root = beast::parse_strict(doc, json_str);  // throws on any RFC violation
```

Rejects: trailing commas · leading zeros · bare decimals · incomplete escapes · lone surrogates · control characters · trailing garbage · empty input.

---

## Language Bindings

### C API

```c
BJSONDocument* doc  = bjson_doc_create();
BJSONValue*    root = bjson_parse(doc, json, len);
int64_t id; bjson_as_int(bjson_get_key(doc, root, "id", 2), &id);
char*   out = bjson_dump(doc, root, &out_len);  // caller free()s
bjson_doc_destroy(doc);
```

Build: `cmake -DBEAST_JSON_BUILD_BINDINGS=ON`. See [docs/API_REFERENCE.md#c-api](docs/API_REFERENCE.md#c-api-bindingsc) for full reference.

### Python

```python
from beast_json import Document, loads

doc  = Document('{"name":"Alice","tags":["go","cpp"]}')
root = doc.root()
print(root["name"])           # Alice
print(root.at("/tags/1"))     # cpp  (RFC 6901)
root["name"].set("Bob")
print(root.dump(indent=2))

data = loads('[1, 2, {"x": 3}]')   # drop-in for json.loads()
```

Build: `cmake -DBEAST_JSON_BUILD_BINDINGS=ON`. See [docs/API_REFERENCE.md#python-binding](docs/API_REFERENCE.md#python-binding-bindingspython) for full reference.

---

## How It Works

Beast JSON stores every JSON token in a **flat, contiguous TapeNode array** (8 bytes/node) inside a single pre-allocated arena:

```
 31      24 23     16 15            0
 ┌────────┬─────────┬───────────────┐
 │  type  │   sep   │    length     │  meta (32-bit)
 └────────┴─────────┴───────────────┘
 ┌────────────────────────────────────┐
 │            byte offset             │  offset (32-bit) → zero-copy strings
 └────────────────────────────────────┘
```

The `sep` field pre-computes the `,`/`:` separator at parse time — the serializer's inner loop becomes a straight memory scan with no state machine. String data is never copied; `offset` points into the original source buffer.

On x86_64, files ≤ 2 MB go through a **two-phase AVX-512 pipeline**: Stage 1 builds a structural positions array at 64 bytes/iteration; Stage 2 walks the positions array with O(1) string lengths. On AArch64, a single-pass SWAR + NEON path is used instead (two-phase NEON movemask emulation costs more than it saves).

**Deep dive:** [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

---

## Security

Beast JSON is hardened via AddressSanitizer, UndefinedBehaviorSanitizer, and libFuzzer. **5 memory-safety bugs** discovered and fixed — heap-buffer-overflows (4) and a UBSan index underflow (1). All fixes are `BEAST_UNLIKELY`-gated with no measurable performance regression.

Three fuzz targets: `fuzz_parse` · `fuzz_lazy` · `fuzz_rfc8259` (consistency oracle).

Full report: [SECURITY.md](SECURITY.md)

---

## Documentation

| Guide | Contents |
|:---|:---|
| [**Getting Started**](docs/GETTING_STARTED.md) | Install, first parse, access patterns, mutation, auto-serialization, pitfalls |
| [**API Reference**](docs/API_REFERENCE.md) | Every method on `Value`, `Document`, `SafeValue`; C API; Python API |
| [**Architecture & Internals**](docs/ARCHITECTURE.md) | TapeNode layout, Stage 1/2 AVX-512, SWAR, KeyLenCache, LUT state machine |
| [**Performance Guide**](docs/PERFORMANCE.md) | Full benchmark tables (3 platforms), PGO setup, tuning knobs, known limits |
| [**Roadmap**](docs/ROADMAP.md) | v1.0 milestones, phase history, platform strategy |
| [**Security Report**](SECURITY.md) | 5 ASan/UBSan/libFuzzer vulnerabilities and fixes |

---

## Test Coverage

368 tests across 43 suites — 100% PASS (GCC 13.3, Release, ASan+UBSan):

```
LazyTypes · LazyRoundTrip · ValueAccessors · ValueMutation · ValueAssign
SafeValue · Monadic · Concepts · StructuralMutation · Iteration · SubtreeDump
PrettyPrint · AutoChain · Ranges · Contains · ValueDefault · TypeName
PipeFallback · KeysValues · AsArray · JsonPointer · JsonPointerCT
Merge · MergePatch · StructBinding · IsValid · AutoSerial · MacroFields
RFC8259_Accept(16) · RFC8259_Reject(29) · RFC8259_ImplDefined · RFC8259_Roundtrip · RFC8259_API
+ Unicode · StrictNumber · ControlChar · Comments · TrailingCommas · DuplicateKeys · ErrorHandling · Serializer · Utf8Validation
```

---

## Related Projects

- [yyjson](https://github.com/ibireme/yyjson) — fastest C JSON library (Beast's primary benchmark target)
- [simdjson](https://github.com/simdjson/simdjson) — SIMD-accelerated C++ JSON, two-phase architecture inspiration
- [nlohmann/json](https://github.com/nlohmann/json) — the gold standard for C++ JSON developer experience
- [Glaze](https://github.com/stephenberry/glaze) — C++23 compile-time reflection JSON
