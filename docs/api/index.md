# API Reference

A complete reference for all public APIs in Beast JSON v1.0.5.

> [!TIP]
> Can't find what you need here? Check the full [Doxygen Reference](/api/reference/index.html){target="_blank"} for auto-generated C++ class documentation.

---

## 🏛️ Core Types

### `beast::Document`

The **owner** of all parsed JSON data. It holds the tape arena and must outlive all `Value` objects derived from it.

```cpp
#include <beast_json/beast_json.hpp>

// Create a document and parse into it
beast::Document doc;
beast::Value root = beast::parse(doc, R"({"name": "Alice"})");

// Re-parse into the same document (reuses capacity — zero malloc after warmup)
beast::Value root2 = beast::parse(doc, R"({"name": "Bob"})");
// ⚠️ root is now invalid! Always use the latest returned Value.
```

---

### `beast::Value`

A lightweight **16-byte handle** into the tape. It supports type checking, typed access, DOM navigation, iteration, and mutation.

---

### `beast::SafeValue`

A **never-throws proxy** created via `Value::get()`. Propagates the "absent" state through entire access chains. Use for untrusted or optional data.

```cpp
// Created via .get() — entire chain will never throw
auto city = root.get("address").get("city").value_or(std::string{"Unknown"});
```

---

## 🔵 Free Functions

### `beast::parse(doc, json)` — Parse JSON String

```cpp
beast::Document doc;
beast::Value root = beast::parse(doc, R"({"id": 1, "active": true})");
```

| Parameter | Type | Description |
| :--- | :--- | :--- |
| `doc` | `Document&` | The document that will own the tape. |
| `json` | `std::string_view` | The JSON text to parse. |

**Returns**: `beast::Value` pointing at the root of the parsed document.  
**Throws**: `std::runtime_error` if JSON is malformed.

---

### `beast::parse_strict(doc, json)` — RFC 8259 Strict Parse

Performs strict RFC 8259 validation before parsing. Rejects trailing commas, leading zeros, lone surrogates, and more.

```cpp
beast::Document doc;
try {
    auto root = beast::parse_strict(doc, R"({"key": "value"})"); // OK
    auto bad  = beast::parse_strict(doc, "[1, 2,]");             // throws!
} catch (const std::runtime_error& e) {
    std::cerr << e.what() << "\n"; // RFC 8259 violation at offset 7
}
```

---

### `beast::rfc8259::validate(json)` — Validate Only (No Parse)

Validates JSON text without building a DOM. Throws `std::runtime_error` on violation.

```cpp
try {
    beast::rfc8259::validate(R"({"valid": true})");  // OK — no throw
    beast::rfc8259::validate("[1, 2,]");              // throws
} catch (const std::runtime_error& e) {
    std::cerr << e.what() << "\n";
}
```

---

### `beast::read<T>(json)` — Deserialize Directly to C++ Type

The highest-level API. Combines parsing and deserialization in one call.

```cpp
// Scalars
int    n = beast::read<int>("42");
double d = beast::read<double>("3.14");
bool   b = beast::read<bool>("true");

// STL containers
auto vec = beast::read<std::vector<int>>("[1, 2, 3]");
auto map = beast::read<std::map<std::string, int>>(R"({"a":1,"b":2})");
auto opt = beast::read<std::optional<int>>("null");    // → std::nullopt
auto tup = beast::read<std::tuple<int, std::string>>("[42, \"hi\"]");

// Custom struct (requires BEAST_JSON_FIELDS)
struct User { std::string name; int age; };
BEAST_JSON_FIELDS(User, name, age)
auto user = beast::read<User>(R"({"name": "Alice", "age": 30})");
```

---

### `beast::write(value)` — Serialize Any Type to JSON String

```cpp
// Scalars
std::string s1 = beast::write(42);           // "42"
std::string s2 = beast::write(3.14);         // "3.14"
std::string s3 = beast::write(true);         // "true"
std::string s4 = beast::write(nullptr);      // "null"
std::string s5 = beast::write("hello");      // "\"hello\""

// STL containers
beast::write(std::vector<int>{1, 2, 3});           // "[1,2,3]"
beast::write(std::map<std::string,int>{{"a",1}});  // "{\"a\":1}"
beast::write(std::optional<int>{});                 // "null"
beast::write(std::tuple{1, "x", true});            // "[1,\"x\",true]"
beast::write(std::pair{"key", 99});                 // "[\"key\",99]"

// Custom struct
struct Config { std::string host; int port; };
BEAST_JSON_FIELDS(Config, host, port)
Config cfg{"localhost", 8080};
beast::write(cfg); // "{\"host\":\"localhost\",\"port\":8080}"
```

### `beast::write(value, indent)` — Pretty Print

```cpp
beast::write(cfg, 2);  // 2-space indented JSON
beast::write(cfg, 4);  // 4-space indented JSON
```

### `beast::write_to(buffer, value)` — Append to Existing Buffer (Zero-Alloc)

For hot loops where you want to **reuse** a pre-allocated string buffer:

```cpp
std::string buf;
buf.reserve(4096);

for (auto& event : live_events) {
    buf.clear();
    beast::write_to(buf, event);   // no heap allocation after warmup!
    send_to_network(buf);
}
```

---

## 🟢 `Value` — Type Checkers

All type checkers return `bool` and **never throw**. They correctly reflect any in-place mutations.

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"n": 42, "s": "hi", "arr": [1,2], "obj": {}})");

auto v = root["n"];
v.is_valid();    // true  — Value points to a real node
v.is_null();     // false — not null
v.is_bool();     // false
v.is_int();      // true  — 42 is an integer
v.is_double();   // false
v.is_number();   // true  — int is a number
v.is_string();   // false

root["s"].is_string();    // true
root["arr"].is_array();   // true
root["obj"].is_object();  // true

// Missing key
root["missing"].is_valid();  // false
root["missing"].is_int();    // false — never crashes

// Human-readable type name
std::string_view tname = root["n"].type_name(); // "int"
```

---

## 🟢 `Value` — Typed Access

### `.as<T>()` — Strict (Throws on Mismatch)

```cpp
auto root = beast::parse(doc, R"({"id":42,"score":9.87,"tag":"vip","flag":true})");

int64_t      id    = root["id"].as<int64_t>();
double       score = root["score"].as<double>();
std::string  tag   = root["tag"].as<std::string>();
bool         flag  = root["flag"].as<bool>();

// Zero-copy view — valid as long as doc is alive
std::string_view sv = root["tag"].as<std::string_view>();

// Throws std::runtime_error on type mismatch or missing key
```

### Implicit Conversion — `operator T()`

```cpp
int         id    = root["id"];     // operator int()
double      score = root["score"];  // operator double()
std::string tag   = root["tag"];    // operator std::string()
bool        flag  = root["flag"];   // operator bool()
```

### `.try_as<T>()` — Non-Throwing (Returns `std::optional<T>`)

```cpp
std::optional<int>    id    = root["id"].try_as<int>();
std::optional<double> score = root["score"].try_as<double>();

if (id.has_value()) std::cout << "ID: " << *id << "\n";
```

### `| default` — Pipe Fallback (Never Throws)

```cpp
int    id    = root["id"]    | -1;
double score = root["score"] | 0.0;
bool   flag  = root["flag"]  | false;
std::string tag = root["tag"] | std::string{"unknown"};
```

---

## 🟢 `Value` — Object Navigation

### `operator[](key)` — Key Lookup (No-Throw)

Returns an invalid `Value{}` when the key is missing (check with `is_valid()`).

```cpp
auto user = root["user"]["profile"]["name"]; // safe even if "user" is missing
if (user.is_valid()) { /* ... */ }
```

### `.find(key)` — Returns `std::optional<Value>`

Distinguishes between "key absent" and "wrong type":

```cpp
if (auto cfg = root.find("config")) {
    int timeout = cfg->value("timeout", 5000);
}
// cfg == std::nullopt means the key doesn't exist at all
```

### `.contains(key)` — Key Existence

```cpp
if (root.contains("optional_field")) {
    process(root["optional_field"]);
}
```

### `.value(key, default)` — Access with Default

```cpp
int    port = root.value("port", 8080);
double ratio = root.value("ratio", 1.0);
std::string mode = root.value("mode", std::string{"fast"});
```

### `.get(key)` — Start a Safe Monadic Chain (`SafeValue`)

```cpp
// Never throws — propagates absent state
std::string city = root.get("user").get("address").get("city").value_or(std::string{"Seoul"});
int port = root.get("server").get("port") | 8080;
```

### `.at(json_pointer)` — RFC 6901 JSON Pointer

```cpp
auto root = beast::parse(doc, R"({"store": {"books": [{"title": "C++ Primer"}]}})");

// Runtime JSON Pointer (string)
std::string t = root.at("/store/books/0/title").as<std::string>();
// "C++ Primer"

// Compile-time JSON Pointer (zero runtime overhead)
using namespace beast::literals;
auto title = root.at("/store/books/0/title"_jptr).as<std::string>();
```

---

## 🟢 `Value` — Array Navigation

### `operator[](index)` — Index Access

```cpp
auto root = beast::parse(doc, R"({"tags": ["cpp", "simd", "hft"]})");
std::string first = root["tags"][0u].as<std::string>(); // "cpp"
std::string third = root["tags"][2u].as<std::string>(); // "hft"
```

### `.size()` — Element Count

```cpp
size_t n = root["tags"].size(); // 3
```

### `.empty()` — Empty Check

```cpp
bool is_empty = root["tags"].empty(); // false
```

---

## 🟢 `Value` — Iteration

### `.items()` — Object Key-Value Pairs

```cpp
auto root = beast::parse(doc, R"({"a": 1, "b": 2, "c": 3})");

for (auto [key, val] : root.items()) {
    std::cout << key << " = " << val.as<int>() << "\n";
}

// Keys and values separately
for (std::string_view key : root.keys()) { /* ... */ }
for (auto val : root.values()) { /* ... */ }
```

### `.elements()` — Array Elements

```cpp
auto root = beast::parse(doc, R"({"scores": [95, 87, 100]})");

for (auto elem : root["scores"].elements()) {
    std::cout << elem.as<int>() << " ";  // 95 87 100
}
```

### `.as_array<T>()` — Typed Array View (Zero Allocation)

```cpp
for (int score : root["scores"].as_array<int>()) {
    std::cout << score << "\n";
}
```

### `.try_as_array<T>()` — Optional Typed Array (Non-Throwing)

```cpp
for (auto v : root["scores"].try_as_array<int>()) {
    if (v) std::cout << *v << "\n";
}
```

### C++20 Ranges

```cpp
#include <ranges>

auto high_scores = root["scores"].elements()
    | std::views::filter([](auto v){ return v.as<int>() >= 90; })
    | std::views::transform([](auto v){ return v.as<int>(); });

auto max_val = std::ranges::max(root["scores"].as_array<int>());
```

---

## 🟡 `Value` — Serialization

### `.dump()` — Serialize to `std::string` (Compact)

```cpp
std::string json   = root.dump();            // whole document
std::string subtree = root["user"].dump();   // subtree only
std::string scalar  = root["id"].dump();     // "42"
```

### `.dump(indent)` — Pretty Print

```cpp
std::string pretty = root.dump(2);  // 2-space indent
std::string pretty4 = root.dump(4); // 4-space indent
```

### `.dump(buffer)` — Append to Existing Buffer (Reuse)

```cpp
std::string buf;
buf.reserve(1024);

for (int i = 0; i < 1'000'000; ++i) {
    root["counter"] = i;
    root.dump(buf);   // zero extra allocation after first call
    send(buf);
}
```

---

## 🔴 `Value` — Mutations

Beast JSON uses **non-destructive mutations**: the original tape is immutable. Changes live in a fast overlay map.

### Scalar Mutation — `set(v)` / `operator=(v)`

```cpp
auto root = beast::parse(doc, R"({"id": 1, "name": "Alice", "score": 87.5})");

root["id"]    = 99;           // integer
root["name"]  = "Bob";        // string
root["score"] = 100.0;        // double
root["active"] = true;        // bool
root["data"]   = nullptr;     // null

// Explicit .set() form (identical behaviour)
root["id"].set(99);
root["name"].set("Bob");

std::cout << root.dump() << "\n";
// {"id":99,"name":"Bob","score":100.0,"active":true,"data":null}
```

### `.unset()` — Restore Original Parsed Value

```cpp
root["id"].unset();
std::cout << root["id"].as<int>(); // 1 (original restored)
```

### `.insert(key, value)` — Add Object Key

```cpp
root.insert("version", 2);
root.insert("label", std::string_view{"preview"});
root.insert("debug", false);
root.insert("ptr", nullptr);
```

### `.insert_json(key, raw_json)` — Add Key from Raw JSON

```cpp
root.insert_json("meta", R"({"build": "release", "arch": "x86_64"})");
root.insert_json("tags", R"(["cpp20", "performance"])");
```

### `.erase(key)` / `.erase(idx)` — Remove Key or Element

```cpp
root.erase("deprecated");       // remove object key
root["tags"].erase(size_t{0});  // remove first array element
```

### `.push_back(value)` — Append to Array

```cpp
root["tags"].push_back(std::string_view{"new_tag"});
root["tags"].push_back(true);
root["tags"].push_back(42);
root["tags"].push_back(nullptr);
```

### `.push_back_json(raw_json)` — Append from Raw JSON

```cpp
root["users"].push_back_json(R"({"id": 3, "name": "Charlie"})");
root["ids"].push_back_json("99");
```

### `.merge(other)` / `.merge_patch(json)` — RFC 7396 Merge Patch

```cpp
auto root = beast::parse(doc, R"({"a": 1, "b": 2, "c": 3})");

// Merge: fields in `other` override/add fields; null removes
root.merge_patch(R"({"b": 99, "c": null, "d": "new"})");

std::cout << root.dump() << "\n";
// {"a":1,"b":99,"d":"new"}  ("c" removed because it was null in patch)
```

### `.patch(json_array)` — RFC 6902 JSON Patch

```cpp
bool ok = root.patch(R"([
    {"op": "add",     "path": "/version", "value": 2},
    {"op": "replace", "path": "/name",    "value": "Bob"},
    {"op": "remove",  "path": "/legacy"}
])");

if (!ok) std::cerr << "Patch failed\n";
```

---

## 🟣 `SafeValue` — Monadic Chain

`SafeValue` is created via `.get()` on any `Value`. It **never throws** under any circumstances.

```cpp
// Object traversal
std::string city = root.get("user").get("address").get("city")
                       .value_or(std::string{"Unknown"});

// Array traversal
int first = root.get("scores").get(0).value_or(0);

// Pipe shorthand
int port = root.get("server").get("port") | 8080;
double lat = root.get("location").get("lat") | 0.0;

// Presence check
if (auto sv = root.get("optional_key")) {
    std::cout << sv.value_or("") << "\n";
}
```

---

## 🟣 `BEAST_JSON_FIELDS` Macro

Auto-generates `from_beast_json` / `to_beast_json` free functions at compile time. Place it **outside** the struct definition.

```cpp
struct Address {
    std::string street;
    std::string city;
    std::string country;
};
BEAST_JSON_FIELDS(Address, street, city, country)   // ← outside!

struct User {
    uint64_t    id;
    std::string username;
    Address     address;                              // nested — auto-recursive
    std::vector<std::string> tags;                   // STL — auto
    std::optional<double>    score;                  // null when empty
    bool        active = true;
};
BEAST_JSON_FIELDS(User, id, username, address, tags, score, active)

// Deserialize
auto user = beast::read<User>(R"({
    "id": 1, "username": "alice",
    "address": {"street": "Main St", "city": "Seoul", "country": "KR"},
    "tags": ["admin"], "score": 99.5, "active": true
})");

// Serialize
std::string json = beast::write(user);
std::string pretty = beast::write(user, 2); // pretty print
```

**Rules:**
- Missing JSON fields keep their C++ **default values** (not an error)
- JSON `null` on a non-optional field is **silently skipped**
- Extra JSON fields not in the struct are **ignored**
- Supports up to **32 fields** per struct
- Works **recursively** for nested structs

---

## 🟣 ADL Hooks for Third-Party Types

If you cannot modify a struct (e.g., from an external library), define these two functions in the **same namespace** as the type:

```cpp
namespace glm {
    // Teach Beast JSON to parse glm::vec3 from a JSON array
    inline void from_beast_json(const beast::Value& v, vec3& out) {
        out.x = v[0u].as<float>();
        out.y = v[1u].as<float>();
        out.z = v[2u].as<float>();
    }

    // Teach Beast JSON to serialize glm::vec3 to a JSON array
    inline void to_beast_json(beast::Value& root, const vec3& in) {
        root = beast::Value::array();
        root.push_back(in.x);
        root.push_back(in.y);
        root.push_back(in.z);
    }
}

// Now glm::vec3 works natively!
glm::vec3 pos = beast::read<glm::vec3>("[1.0, 2.0, 3.5]");
std::string json = beast::write(pos); // "[1.0,2.0,3.5]"
```

---

## ⚡ Quick Reference Table

| API | Throws? | Returns | Use When |
| :--- | :---: | :--- | :--- |
| `beast::parse(doc, s)` | ✅ | `Value` | Standard parsing |
| `beast::parse_strict(doc, s)` | ✅ | `Value` | Need RFC 8259 compliance |
| `beast::rfc8259::validate(s)` | ✅ | `void` | Validation without DOM |
| `beast::read<T>(s)` | ✅ | `T` | Deserialize directly |
| `beast::write(v)` | ❌ | `std::string` | Serialize any type |
| `beast::write(v, indent)` | ❌ | `std::string` | Pretty print |
| `beast::write_to(buf, v)` | ❌ | `void` | Zero-alloc hot loops |
| `v.as<T>()` | ✅ | `T` | Confident typed access |
| `v.try_as<T>()` | ❌ | `optional<T>` | Safe typed access |
| `v \| default` | ❌ | `T` | Access with fallback |
| `v.get("key")` | ❌ | `SafeValue` | Deep optional chain |
| `v["key"]` | ❌ | `Value` | Object key lookup |
| `v.find("key")` | ❌ | `optional<Value>` | Distinguish absent vs wrong type |
| `v.at("/a/b")` | ❌ | `Value` | JSON Pointer access |
| `v.dump()` | ❌ | `std::string` | Serialize subtree |
| `v.erase(key)` | ❌ | `bool` | Remove object key |
| `v.insert(key, val)` | ❌ | `void` | Add object key |
| `v.push_back(val)` | ❌ | `void` | Append array element |
| `v.merge_patch(json)` | ❌ | `void` | RFC 7396 merge |
| `v.patch(json)` | ❌ | `bool` | RFC 6902 JSON Patch |
