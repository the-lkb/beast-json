# Parsing & Access

Beast JSON uses a **lazy DOM model** — it parses into a flat tape in a single pass, and you access values on demand. It's designed to be both fast and easy to use.

## 🚀 Quick Start

```cpp
#include <beast_json/beast_json.hpp>

int main() {
    std::string json = R"({"name": "Alice", "age": 30, "active": true})";

    beast::Document doc;
    beast::Value root = beast::parse(doc, json);

    std::string name = root["name"].as<std::string>(); // Alice
    int         age  = root["age"].as<int>();           // 30
    bool        flag = root["active"].as<bool>();       // true
}
```

## 📦 Parsing Entry Points

### `beast::parse(doc, json)` — Standard Parse

The primary API. Takes a `Document` (which owns the tape memory) and a `string_view`.

```cpp
beast::Document doc;
beast::Value root = beast::parse(doc, R"({"key": "value"})");
```

> [!IMPORTANT]
> `Document` must **outlive** all `Value` objects derived from it. Values are lightweight 16-byte handles pointing into the document's tape.

### `beast::parse_reuse(doc, json)` — Explicit Reuse for Hot Loops

For high-frequency parsing (e.g., JSON streams), use `beast::parse_reuse()` on the same document. It resets the tape and mutation overlays without reallocating heap memory, making the intent self-documenting in hot-loop code.

```cpp
beast::Document doc;
doc.reserve(4 * 1024); // optional: pre-warm to avoid first-parse realloc
std::string line;

while (std::getline(socket_stream, line)) {
    // Re-uses the previously allocated tape capacity — zero extra malloc
    beast::Value root = beast::parse_reuse(doc, line);
    if (!root.is_valid()) continue;
    process(root);
}
```

> [!NOTE]
> `beast::parse()` and `beast::parse_reuse()` behave identically when called on the same `Document`. `parse_reuse()` is provided as a self-documenting alias that makes reuse intent explicit.

### `beast::parse_strict(doc, json)` — RFC 8259 Strict Mode

Throws `std::runtime_error` on any RFC 8259 violation: trailing commas, leading zeros, lone surrogates, etc.

```cpp
try {
    beast::Document doc;
    auto root = beast::parse_strict(doc, "[1, 2,]"); // throws!
} catch (const std::runtime_error& e) {
    // "RFC 8259 violation at offset 6: trailing comma"
    std::cerr << e.what() << "\n";
}
```

### `beast::read<T>(json)` — Deserialize Directly to a C++ Type

The most convenient API for known types. Combines parsing and deserialization in one call.

```cpp
// STL containers
auto ids   = beast::read<std::vector<int>>("[1, 2, 3]");
auto kv    = beast::read<std::map<std::string, int>>(R"({"a":1,"b":2})");
auto tuple = beast::read<std::tuple<int, std::string, bool>>(R"([42, "hi", true])");

// Custom structs (with BEAST_JSON_FIELDS)
auto user = beast::read<User>(R"({"name": "Alice", "age": 30})");
```

---

## 🎯 Accessing Values

### Scalar Access with `.as<T>()`

Type-safe access that throws `std::runtime_error` on type mismatch.

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"id": 101, "score": 9.87, "tag": "vip", "active": true})");

int64_t id     = root["id"].as<int64_t>();
double  score  = root["score"].as<double>();
std::string tag = root["tag"].as<std::string>();
bool    active = root["active"].as<bool>();

// Zero-copy string view (valid as long as doc is alive)
std::string_view sv = root["tag"].as<std::string_view>();
```

### Implicit Conversion (Ergonomic Shorthand)

```cpp
// Implicit conversions via operator T()
int         id    = root["id"];     // operator int()
double      score = root["score"];  // operator double()
std::string tag   = root["tag"];    // operator std::string()
bool        flag  = root["active"]; // operator bool()
```

### Non-Throwing Access with `.try_as<T>()`

Returns `std::optional<T>` — never throws.

```cpp
std::optional<int>    id    = root["id"].try_as<int>();
std::optional<double> score = root["score"].try_as<double>();

if (id) std::cout << "ID: " << *id << "\n";
```

### Pipe Fallback `| default_value` — Never Throws

The safest and most ergonomic pattern. Provides a default if the key is missing or the type doesn't match.

```cpp
int      id    = root["id"]      | -1;
double   score = root["score"]   | 0.0;
bool     flag  = root["active"]  | false;
std::string name = root["name"]  | std::string{"anonymous"};
```

### Safe Monadic Chains with `.get()`

Perfect for deeply nested access without checking every level. Never throws.

```cpp
auto root = beast::parse(doc, R"({
    "user": { "profile": { "city": "Seoul" } }
})");

// Propagates "absent" state through entire chain
std::string city = root.get("user").get("profile").get("city").value_or("Unknown");
// → "Seoul"

// If any key is missing, returns the default:
std::string zip = root.get("user").get("profile").get("zip").value_or("00000");
// → "00000" (no exception!)
```

---

## 🔍 Type Checkers

All type checkers return `bool` and **never throw**.

```cpp
auto v = root["data"];

v.is_valid();    // false if key doesn't exist
v.is_object();   // {"a": 1}
v.is_array();    // [1, 2, 3]
v.is_string();   // "hello"
v.is_int();      // 42
v.is_double();   // 3.14
v.is_bool();     // true / false
v.is_null();     // null
v.is_number();   // is_int() || is_double()

// Get a human-readable name of the type
std::string_view tname = v.type_name(); // "int", "string", "array", etc.
```

```cpp
// Pattern: check-then-access
if (root["count"].is_int()) {
    std::cout << "Count: " << root["count"].as<int>() << "\n";
}
```

---

## 🔑 Object Access

### Key Lookup with `find()`

Returns `std::optional<Value>`, distinguishing between "key absent" and "key with wrong type".

```cpp
if (auto config = root.find("config")) {
    int timeout = config->value("timeout", 5000);
    std::string mode = config->value("mode", std::string{"fast"});
}
```

### `contains(key)` — Key Existence Check

```cpp
if (root.contains("optional_field")) {
    process(root["optional_field"]);
}
```

### `value(key, default)` — Access with Default

```cpp
// Never throws. Returns default if key absent or wrong type.
int    timeout = root.value("timeout", 5000);
double ratio   = root.value("ratio", 1.0);
std::string mode = root.value("mode", std::string{"default"});
```

---

## 📋 Iterating Collections

### Object Iteration (Key-Value Pairs)

```cpp
auto root = beast::parse(doc, R"({"a": 1, "b": 2, "c": 3})");

// Structured bindings (C++20)
for (auto [key, val] : root.items()) {
    std::cout << key << " = " << val.as<int>() << "\n"; // a=1, b=2, c=3
}

// Keys only
for (std::string_view key : root.keys()) { /* ... */ }

// Values only
for (auto val : root.values()) { /* ... */ }
```

### Array Iteration

```cpp
auto root = beast::parse(doc, R"({"scores": [95, 87, 100]})");

// Generic element access
for (auto elem : root["scores"].elements()) {
    std::cout << elem.as<int>() << " ";  // 95 87 100
}

// Typed view — zero allocation
for (int score : root["scores"].as_array<int>()) {
    std::cout << score << " ";  // 95 87 100
}
```

### C++20 Ranges

```cpp
#include <ranges>
auto scores = root["scores"].elements();

// Filter + transform pipeline
auto high_scores = scores
    | std::views::filter([](auto v){ return v.as<int>() >= 90; })
    | std::views::transform([](auto v){ return v.as<int>(); });

for (int s : high_scores) std::cout << s << " ";  // 95 100

// Algorithm
auto max_it = std::ranges::max_element(scores, {}, [](auto v){ return v.as<int>(); });
std::cout << "Best: " << max_it->as<int>() << "\n";  // 100
```

---

## 📌 JSON Pointer (RFC 6901)

Access deeply nested values using a path string.

```cpp
auto root = beast::parse(doc, R"({
    "store": { "book": [{"title": "C++ Primer"}, {"title": "Effective C++"}] }
})");

// Runtime JSON Pointer
std::string t = root.at("/store/book/0/title").as<std::string>(); // "C++ Primer"

// Compile-time JSON Pointer (zero runtime overhead)
using namespace beast::literals;
auto title = root.at("/store/book/1/title"_jptr).as<std::string>(); // "Effective C++"
```

---

## ⚠️ Common Pitfalls

### 1. Doc Must Outlive Values

```cpp
// ❌ WRONG — doc is destroyed before root is used
beast::Value bad_parse() {
    beast::Document doc;
    return beast::parse(doc, R"({"x": 1})");
} // doc destroyed here → dangling pointer!

// ✅ CORRECT — keep them together
struct ParseResult {
    beast::Document doc;
    beast::Value root;
};
ParseResult good_parse(std::string_view json) {
    ParseResult r;
    r.root = beast::parse(r.doc, json);
    return r;
}
```

### 2. Array Indices — Plain `int` Works

All array index APIs (`operator[]`, `get()`, `erase()`) accept plain `int`, `unsigned int`, and `size_t`. No cast is needed:

```cpp
auto elem  = root["array"][0];   // ✅ plain int literal — works fine
auto elem2 = root["array"][1];   // ✅ same

int i = 2;
auto elem3 = root["array"][i];   // ✅ int variable — also works, no cast needed
```

Negative indices simply return an invalid (absent) value rather than throwing or invoking undefined behaviour:

```cpp
auto bad = root["array"][-1];    // ✅ safe — returns invalid Value{}
bad.is_valid();                   // false
```
