# Beast JSON — Getting Started

> C++20 · Single Header · CMake 3.14+

---

---

## Requirements

| Requirement | Minimum | Recommended |
|:---|:---|:---|
| C++ Standard | C++20 | C++20 |
| GCC | 10 | 13.3 |
| Clang | 10 | 17+ |
| MSVC | 19.29 (VS 2019 16.10) | 19.40+ |
| CMake | 3.14 | 3.28 |
| OS | Linux, macOS, Windows | — |

**Optional for best performance:**
- AVX-512 CPU (Intel Skylake-X+, Ice Lake+) — enables 64B/iter whitespace skip and two-phase parsing.
- PGO (Profile-Guided Optimization) — see [docs/OPTIMIZATION_PLAN.md](OPTIMIZATION_PLAN.md).

---

## Installation

### Option A: Single Header Drop-in

Beast JSON is a **single header library**. Copy the header into your project — no CMake required.

```bash
cp include/beast_json/beast_json.hpp /your/project/include/
```

```cpp
#include "beast_json.hpp"
```

Compile with C++20:

```bash
g++ -std=c++20 -O3 main.cpp -o main
```

### Option B: CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    beast_json
    GIT_REPOSITORY https://github.com/kyubuem/beast-json
    GIT_TAG        main
)
FetchContent_MakeAvailable(beast_json)

target_link_libraries(your_target PRIVATE beast_json)
```

### Option C: Clone & Build

```bash
git clone https://github.com/kyubuem/beast-json.git
cd beast-json

# Release build with tests
cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DBEAST_JSON_BUILD_TESTS=ON
cmake --build build -j$(nproc)

# Run all 368 tests
ctest --test-dir build --output-on-failure
```

**CMake install (for system-wide use):**

```bash
cmake --install build --prefix /usr/local
```

---

## First Parse

```cpp
#include <beast_json/beast_json.hpp>
#include <iostream>

int main() {
    std::string_view json = R"({
        "name": "Beast JSON",
        "version": 1,
        "fast": true,
        "tags": ["c++20", "zero-copy", "simd"]
    })";

    beast::Document doc;
    beast::Value root = beast::parse(doc, json);

    std::cout << root["name"].as<std::string>() << "\n"; // Beast JSON
    std::cout << root["version"].as<int>()       << "\n"; // 1
    std::cout << root["fast"].as<bool>()          << "\n"; // 1 (true)

    return 0;
}
```

**Key rules:**
- `beast::Document` owns the memory arena. It must **outlive** all `Value` objects derived from it.
- `beast::parse()` returns a `Value` pointing into the `doc`'s tape. The `doc` can be reused by calling `parse()` again (tape reset, overlays cleared).
- `Value` is a lightweight (16-byte) handle — cheap to copy and pass by value.

---

## Reading Values

### Type-checked access (throws on mismatch)

```cpp
// Scalar types
int    id     = root["id"].as<int>();
double score  = root["score"].as<double>();
bool   active = root["active"].as<bool>();
std::string name = root["name"].as<std::string>();

// Zero-copy string view (valid while doc is alive)
std::string_view sv = root["name"].as<std::string_view>();

// Type checks
root["id"].is_int()     // true
root["id"].is_string()  // false
root["id"].type_name()  // "int"

// Validity
root["missing"].is_valid()  // false — missing key returns invalid Value{}
```

### Implicit conversion (nlohmann-style)

```cpp
int    id   = root["id"];       // operator int()
double sc   = root["score"];    // operator double()
std::string name = root["name"]; // operator std::string()
bool   flag = root["active"];   // operator bool()
```

### Non-throwing access (`try_as`)

```cpp
std::optional<int>    id    = root["id"].try_as<int>();    // nullopt on mismatch
std::optional<double> score = root["score"].try_as<double>();
```

### Pipe fallback (default values, never throws)

```cpp
int    id   = root["id"]    | -1;           // -1 if missing or wrong type
double sc   = root["score"] | 0.0;
std::string s = root["name"] | std::string{"unknown"};
```

### Nested access

```cpp
// Deep chains — safe even if intermediate keys are missing
int city_len = root["user"]["addr"]["city"].as<std::string>().size();

// Missing at any level → invalid Value{} — no exception
std::string_view missing = root["a"]["b"]["c"] | std::string_view{"default"};
```

---

## Safe Access Patterns

When you need to distinguish "key absent" from "key with wrong type" without exceptions, use `find()` or `SafeValue`:

### find() — returns optional\<Value\>

```cpp
if (auto v = root.find("config")) {
    int timeout = v->value("timeout", 5000);
} else {
    // "config" key was absent entirely
}
```

### SafeValue — optional-propagating proxy

```cpp
// Never throws, propagates absence through the chain
auto name = root.get("user")["profile"]["name"].as<std::string>();
// → std::optional<std::string>

int timeout = root.get("config")["timeout"].value_or(5000);
int deep    = root.get("a")["b"]["c"].value_or(0);  // → 0 if any step missing

// value_or with a SafeValue using pipe syntax
int n = root.get("n") | 42;  // same as value_or(42)
```

### contains() and value()

```cpp
if (root.contains("optional_field"))
    process(root["optional_field"]);

// value() = operator[] + as<T> + default, never throws
int timeout = root.value("timeout", 5000);
std::string mode = root.value("mode", std::string{"fast"});
```

---

## Iterating Objects and Arrays

### Object iteration

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"a":1,"b":2,"c":3})");

// Structured bindings
for (auto [key, val] : root.items()) {
    std::cout << key << " = " << val.as<int>() << "\n";
}

// keys() and values() separately
for (std::string_view k : root.keys())
    std::cout << k << "\n";

for (auto v : root.values())
    std::cout << v.as<int>() << "\n";
```

### Array iteration

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"ids":[10,20,30],"tags":["go","cpp"]})");

for (auto elem : root["ids"].elements())
    std::cout << elem.as<int>() << " ";  // 10 20 30

// Typed view — zero allocation
for (int id : root["ids"].as_array<int>())
    std::cout << id << " ";  // 10 20 30

// try_as variant — never throws
for (auto v : root["ids"].try_as_array<int>())
    if (v) std::cout << *v << " ";
```

### C++20 Ranges pipelines

```cpp
#include <ranges>

// Filter + transform
auto big_ids = root["ids"].elements()
    | std::views::filter([](auto v){ return v.as<int>() > 15; })
    | std::views::transform([](auto v){ return v.as<int>(); });

for (int id : big_ids)
    std::cout << id << "\n";  // 20, 30

// std::ranges algorithms
auto it = std::ranges::max_element(root["ids"].elements(), {},
    [](auto v){ return v.as<int>(); });
std::cout << it->as<int>() << "\n";  // 30
```

---

## Mutating Documents

### Value mutation (scalar overlay)

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"user":{"id":1,"name":"Alice"}})");

// Both forms are equivalent
root["user"]["id"] = 99;
root["user"]["id"].set(99LL);

root["user"]["name"] = "Bob";
root["user"]["active"] = true;    // can set to any scalar type
root["user"]["score"] = 3.14;
root["user"]["ptr"]   = nullptr;  // null

// Restore original parsed value
root["user"]["id"].unset();

// Immediately reflected in dump()
std::cout << root["user"].dump() << "\n";
// {"id":1,"name":"Bob","active":true,"score":3.14,"ptr":null}
// (id is back to 1 after unset())
```

### Structural mutation (add / remove)

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"users":[{"id":1},{"id":2}],"tags":["a","b"]})");

// Object: add key
root.insert("version", 2);
root.insert("label", std::string_view{"hello"});
root.insert_json("meta", R"({"build":"release"})");

// Object: remove key
root.erase("deprecated_field");

// Array: append
root["tags"].push_back(std::string_view{"c"});
root["users"].push_back_json(R"({"id":3})");

// Array: remove by index
root["tags"].erase(0);   // removes "a"

// All changes reflected immediately
std::cout << root.dump() << "\n";
```

**Important:** The original tape is immutable. Mutations are stored in overlay maps. The document must remain alive for overlays to be valid.

---

## Serialization

### dump() — allocates a new string

```cpp
std::string json = root.dump();            // compact
std::string pretty = root.dump(2);         // 2-space indent
std::string pretty4 = root.dump(4);        // 4-space indent
std::string subtree = root["user"].dump(); // serialize subtree only
```

### dump(out) — reuse existing buffer

For hot loops or repeated serialization of the same document, supply your own buffer. The `last_dump_size_` cache (Phase 75B) makes `resize()` a no-op if the output size is unchanged.

```cpp
std::string buf;
for (int i = 0; i < 1'000'000; ++i) {
    // ... update some fields via set() ...
    root.dump(buf);   // reuses buf capacity — no malloc after first call
    send(buf);
}
```

---

## Auto-Serialization (Structs)

```cpp
#include <beast_json/beast_json.hpp>

struct Address {
    std::string city;
    std::string country;
};
BEAST_JSON_FIELDS(Address, city, country)  // one line — read + write registered

struct User {
    std::string              name;
    int                      age   = 0;
    Address                  addr;                 // nested — auto
    std::vector<std::string> tags;                 // STL — auto
    std::optional<double>    score;                // optional — auto
};
BEAST_JSON_FIELDS(User, name, age, addr, tags, score)

int main() {
    // Deserialize
    auto user = beast::read<User>(R"({
        "name": "Alice", "age": 30,
        "addr": {"city": "Seoul", "country": "KR"},
        "tags": ["admin", "user"],
        "score": 99.5
    })");

    std::cout << user.name << "\n";         // Alice
    std::cout << user.addr.city << "\n";    // Seoul
    std::cout << user.tags[0] << "\n";      // admin

    // Serialize
    std::string json = beast::write(user);

    // STL containers — zero effort (no macro needed)
    auto ids = beast::read<std::vector<int>>("[1,2,3]");
    auto map = beast::read<std::map<std::string,double>>(R"({"pi":3.14})");
    auto tup = beast::read<std::tuple<int,std::string,bool>>("[42,\"ok\",true]");

    std::string j1 = beast::write(std::pair{3, std::string{"hello"}});  // [3,"hello"]
    std::string j2 = beast::write(std::optional<int>{});                 // null
}
```

**BEAST_JSON_FIELDS behavior:**
- Missing JSON field → struct member keeps its C++ default value
- JSON `null` on a non-optional field → skip silently
- Extra JSON fields not in the struct → ignored
- Supports up to 32 fields per struct
- Works recursively for nested structs

---

### Serializing Third-Party Types (ADL)

If you cannot modify a struct (e.g., `glm::vec3` from an external library), you cannot use the `BEAST_JSON_FIELDS` macro. Instead, you can define two **Argument-Dependent Lookup (ADL)** functions inside the *same namespace* as your target type:

```cpp
#include <glm/vec3.hpp>

// 1. Open the namespace of the target type
namespace glm {
    
    // 2. Define the parsing hook: from_beast_json
    inline void from_beast_json(const beast::json::Value& v, vec3& out) {
        // Assuming we want to parse it from a JSON Array: [x, y, z]
        out.x = v[0].as_double();
        out.y = v[1].as_double();
        out.z = v[2].as_double();
    }

    // 3. Define the serialization hook: to_beast_json
    inline void to_beast_json(beast::json::Value& root, const vec3& in) {
        // Construct a JSON array and push the values
        root = beast::json::Value::Array();
        root.push_back(beast::json::Value(in.x));
        root.push_back(beast::json::Value(in.y));
        root.push_back(beast::json::Value(in.z));
    }
}

// Now you can natively read/write glm::vec3 safely
int main() {
    glm::vec3 position = beast::read<glm::vec3>("[1.0, 2.0, 3.5]");
    std::string json = beast::write(position);
}
```

---

## RFC 8259 Strict Mode

By default, `beast::parse()` is lenient. Use `beast::parse_strict()` or `beast::rfc8259::validate()` when you need guaranteed RFC 8259 compliance.

```cpp
#include <beast_json/beast_json.hpp>

int main() {
    // Validation only (no parse)
    try {
        beast::rfc8259::validate("[1,2,]");  // trailing comma → throws
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << "\n";
        // RFC 8259 violation at offset 5: trailing comma
    }

    // Validate + parse in one call
    beast::Document doc;
    try {
        auto root = beast::parse_strict(doc, R"({"key": "value"})");  // OK
        auto bad  = beast::parse_strict(doc, "[1,2,]");              // throws
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << "\n";
    }

    // Rejected inputs:
    // [1,2,]          — trailing comma
    // {"a":01}        — leading zero
    // {"\uD800":"x"}  — lone surrogate
    // [1,2} extra     — trailing garbage
    // (empty string)  — empty input
}
```

---

## Buffer Reuse for Hot Loops

For maximum performance when parsing multiple documents (e.g., a JSON stream), reuse the `Document`:

```cpp
beast::Document doc;
std::string line;

while (std::getline(std::cin, line)) {
    // parse_reuse() resets tape + clears overlays, reuses malloc'd capacity
    beast::Value root = beast::parse(doc, line);
    // process root...
    std::string out;
    root.dump(out);   // reuse out buffer across iterations too
    std::cout << out << "\n";
}
```

**What `beast::parse()` does on reuse:**
1. Clears `mutations_`, `deleted_`, `additions_` overlays
2. Resets `last_dump_size_` to 0
3. Calls `tape.reserve(n)` — if existing capacity ≥ n, just resets `head` (no malloc)
4. Runs Stage 1 + Stage 2 (or single-pass) parser

---

## Build Options Reference

| CMake Option | Default | Description |
|:---|:---:|:---|
| `BEAST_JSON_BUILD_TESTS` | `OFF` | Build test suite (`ctest`) |
| `BEAST_JSON_BUILD_BENCHMARKS` | `OFF` | Build benchmark executables |
| `BEAST_JSON_BUILD_BINDINGS` | `OFF` | Build C shared library (`libbeast_json_c`) |
| `BEAST_JSON_BUILD_FUZZ` | `OFF` | Build libFuzzer targets (requires Clang) |

**Performance builds:**

```bash
# Maximum performance (GCC + PGO requires two-step build)
cmake -S . -B build-pgo-gen \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAST_JSON_BUILD_BENCHMARKS=ON \
    "-DCMAKE_CXX_FLAGS=-fprofile-generate -march=native"
cmake --build build-pgo-gen
./build-pgo-gen/benchmarks/bench_all --all  # generate profile data

cmake -S . -B build-pgo-use \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAST_JSON_BUILD_BENCHMARKS=ON \
    "-DCMAKE_CXX_FLAGS=-fprofile-use -fprofile-correction -march=native"
cmake --build build-pgo-use

# ASan + UBSan (sanitizer build)
cmake -S . -B build-san \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBEAST_JSON_BUILD_TESTS=ON \
    "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-san
ctest --test-dir build-san  # 368/368 PASS expected
```

---

## Running Tests

```bash
# Build and run all 368 tests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBEAST_JSON_BUILD_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

# Run a specific test suite
ctest --test-dir build -R RFC8259

# Verbose output
ctest --test-dir build -V
```

**Test suites included:**

| Suite | Count | What it covers |
|:---|---:|:---|
| LazyTypes, LazyRoundTrip | 30 | Core parsing + serialization round-trips |
| ValueAccessors, ValueMutation | 56 | `as<T>()`, `set()`, mutation operators |
| SafeValue, Monadic | 29 | SafeValue chain, optional propagation |
| Ranges, Iteration | 16 | `items()`, `elements()`, `std::ranges` |
| PipeFallback, ValueDefault | 14 | `operator\|`, `value()` defaults |
| JsonPointer, JsonPointerCT | 10 | RFC 6901 runtime + compile-time |
| MergePatch, Merge | 6 | RFC 7396 merge patch |
| AutoSerial, MacroFields | 33 | `read<T>()`, `write()`, `BEAST_JSON_FIELDS` |
| RFC8259_* | 56 | RFC 8259 strict validation |
| + 17 other suites | 78 | Unicode, escaping, concepts, etc. |

---

## Language Bindings

### C API

```bash
# Build shared library
cmake -S . -B build -DBEAST_JSON_BUILD_BINDINGS=ON
cmake --build build --target beast_json_c

# Output: build/bindings/c/libbeast_json_c.so (Linux)
#         build/bindings/c/libbeast_json_c.dylib (macOS)
```

```c
#include "bindings/c/beast_json_c.h"

BJSONDocument* doc = bjson_doc_create();
BJSONValue* root = bjson_parse(doc, json_str, json_len);

int64_t id;
bjson_as_int(bjson_get_key(doc, root, "id", 2), &id);

bjson_doc_destroy(doc);
```

See [docs/API_REFERENCE.md#c-api](API_REFERENCE.md#c-api-bindingsc) for the complete C API.

### Python

```bash
export BEAST_JSON_LIB_PATH=./build/bindings/c
cd bindings/python
python3 example.py
```

```python
from beast_json import Document, loads

doc = Document('{"name":"Alice","score":99}')
root = doc.root()
print(root["name"])          # Alice
print(root["score"])         # 99.0

# Drop-in for json.loads()
data = loads('[1, 2, {"x": 3}]')
```

See [docs/API_REFERENCE.md#python-binding](API_REFERENCE.md#python-binding-bindingspython) for the complete Python API.

---

## Common Pitfalls

### 1. Document must outlive all Value objects

```cpp
// WRONG — doc destroyed before root is used
beast::Value bad() {
    beast::Document doc;
    return beast::parse(doc, R"({"x":1})");
}  // doc destroyed here → dangling pointer

// CORRECT — keep doc alive alongside the Value
struct ParseResult {
    beast::Document doc;
    beast::Value root;
};

ParseResult parse_it(std::string_view json) {
    ParseResult r;
    r.root = beast::parse(r.doc, json);
    return r;
}
```

### 2. string_view source must remain alive

`beast::parse()` takes a `std::string_view` and zero-copies strings. The underlying buffer must remain alive for the lifetime of the `Document`.

```cpp
// WRONG — temporary string destroyed
beast::Value bad(beast::Document& doc) {
    return beast::parse(doc, std::string{R"({"x":1})"});
    // string destroyed at semicolon — zero-copy pointers dangle
}

// CORRECT — bind to a named variable with sufficient lifetime
std::string json = load_file("data.json");
beast::Document doc;
auto root = beast::parse(doc, json);  // json outlives doc
```

### 3. Reusing a Document clears all mutations

```cpp
beast::Document doc;
auto root = beast::parse(doc, json1);
root["key"] = 42;            // set mutation overlay

auto root2 = beast::parse(doc, json2);  // ← clears all overlays on doc
// root is now invalid! (tape was reset)
// root2 is valid and points to json2's parsed data
```

### 4. int ambiguity — use size_t for array index

```cpp
// Ambiguous: root["key"] with literal 0
auto v = root["key"];   // OK — const char* overload

// For integer indices, use size_t explicitly
auto elem = root["array"][size_t{0}];   // array[0]
auto elem2 = root["array"][0u];         // also fine
```

### 5. as\<int\> on a double returns a cast, not an error

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"x": 3.14})");
int n = root["x"].as<int>();  // returns 3 (truncated cast), does NOT throw
// Use is_int() first if you need to distinguish integer from double
```
