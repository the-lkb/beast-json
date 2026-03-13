# API Reference

A complete reference for all public APIs in qbuem-json v1.0.5.

> [!TIP]
> Looking for auto-generated class/struct documentation? The **[Doxygen Reference](/qbuem-json/api/reference/index.html)** is rebuilt automatically whenever `qbuem_json.hpp` changes and deployed to Pages alongside this guide.

---

## 🏛️ Core Types

### `qbuem::Document`

The **owner** of all parsed JSON data. It holds the tape arena and must outlive all `Value` objects derived from it.

```cpp
#include <qbuem_json/qbuem_json.hpp>

// Create a document and parse into it
qbuem::Document doc;
qbuem::Value root = qbuem::parse(doc, R"({"name": "Alice"})");

// Re-parse into the same document (reuses capacity — zero malloc after warmup)
qbuem::Value root2 = qbuem::parse(doc, R"({"name": "Bob"})");
// ⚠️ root is now invalid! Always use the latest returned Value.
```

---

### `qbuem::Value`

A lightweight **16-byte handle** into the tape. It supports type checking, typed access, DOM navigation, iteration, and mutation.

---

### `qbuem::SafeValue`

A **never-throws proxy** created via `Value::get()`. Propagates the "absent" state through entire access chains. Use for untrusted or optional data.

```cpp
// Created via .get() — entire chain will never throw
auto city = root.get("address").get("city").value_or(std::string{"Unknown"});
```

---

## 🔵 Free Functions

### `qbuem::parse(doc, json)` — Parse JSON String

```cpp
qbuem::Document doc;
qbuem::Value root = qbuem::parse(doc, R"({"id": 1, "active": true})");
```

| Parameter | Type | Description |
| :--- | :--- | :--- |
| `doc` | `Document&` | The document that will own the tape. |
| `json` | `std::string_view` | The JSON text to parse. |

**Returns**: `qbuem::Value` pointing at the root of the parsed document.
**Does not throw.** Malformed or non-standard JSON is parsed on a best-effort basis. For strict RFC 8259 enforcement with exception-based error handling, use `qbuem::parse_strict`.

---

### `qbuem::parse_reuse(doc, json)` — Explicit Hot-Loop Reuse

Identical to `qbuem::parse()` — resets the tape and mutation overlays, then parses. Provided as a self-documenting alias to make reuse intent explicit in high-frequency parsing loops.

```cpp
qbuem::Document doc;
doc.reserve(4 * 1024); // optional: pre-warm

while (running) {
    auto msg = source.receive();
    qbuem::Value root = qbuem::parse_reuse(doc, msg); // zero malloc after warmup
    if (!root.is_valid()) continue;
    dispatch(root);
}
```

---

### `qbuem::parse_strict(doc, json)` — RFC 8259 Strict Parse

Performs strict RFC 8259 validation before parsing. Rejects trailing commas, leading zeros, lone surrogates, and more.

```cpp
qbuem::Document doc;
try {
    auto root = qbuem::parse_strict(doc, R"({"key": "value"})"); // OK
    auto bad  = qbuem::parse_strict(doc, "[1, 2,]");             // throws!
} catch (const std::runtime_error& e) {
    std::cerr << e.what() << "\n"; // RFC 8259 violation at offset 7
}
```

---

### `qbuem::rfc8259::validate(json)` — Validate Only (No Parse)

Validates JSON text without building a DOM. Throws `std::runtime_error` on violation.

```cpp
try {
    qbuem::rfc8259::validate(R"({"valid": true})");  // OK — no throw
    qbuem::rfc8259::validate("[1, 2,]");              // throws
} catch (const std::runtime_error& e) {
    std::cerr << e.what() << "\n";
}
```

---

### `qbuem::read<T>(json)` — Deserialize Directly to C++ Type

The highest-level API. Combines parsing and deserialization in one call.

```cpp
// Scalars
int    n = qbuem::read<int>("42");
double d = qbuem::read<double>("3.14");
bool   b = qbuem::read<bool>("true");

// STL containers
auto vec = qbuem::read<std::vector<int>>("[1, 2, 3]");
auto map = qbuem::read<std::map<std::string, int>>(R"({"a":1,"b":2})");
auto opt = qbuem::read<std::optional<int>>("null");    // → std::nullopt
auto tup = qbuem::read<std::tuple<int, std::string>>("[42, \"hi\"]");

// Custom struct (requires QBUEM_JSON_FIELDS)
struct User { std::string name; int age; };
QBUEM_JSON_FIELDS(User, name, age)
auto user = qbuem::read<User>(R"({"name": "Alice", "age": 30})");
```

---

### `qbuem::write(value)` — Serialize Any Type to JSON String

```cpp
// Scalars
std::string s1 = qbuem::write(42);           // "42"
std::string s2 = qbuem::write(3.14);         // "3.14"
std::string s3 = qbuem::write(true);         // "true"
std::string s4 = qbuem::write(nullptr);      // "null"
std::string s5 = qbuem::write("hello");      // "\"hello\""

// STL containers
qbuem::write(std::vector<int>{1, 2, 3});           // "[1,2,3]"
qbuem::write(std::map<std::string,int>{{"a",1}});  // "{\"a\":1}"
qbuem::write(std::optional<int>{});                 // "null"
qbuem::write(std::tuple{1, "x", true});            // "[1,\"x\",true]"
qbuem::write(std::pair{"key", 99});                 // "[\"key\",99]"

// Custom struct
struct Config { std::string host; int port; };
QBUEM_JSON_FIELDS(Config, host, port)
Config cfg{"localhost", 8080};
qbuem::write(cfg); // "{\"host\":\"localhost\",\"port\":8080}"
```

### `qbuem::write(value, indent)` — Pretty Print

```cpp
qbuem::write(cfg, 2);  // 2-space indented JSON
qbuem::write(cfg, 4);  // 4-space indented JSON
```

### `qbuem::write_to(buffer, value)` — Append to Existing Buffer (Zero-Alloc)

For hot loops where you want to **reuse** a pre-allocated string buffer:

```cpp
std::string buf;
buf.reserve(4096);

for (auto& event : live_events) {
    buf.clear();
    qbuem::write_to(buf, event);   // no heap allocation after warmup!
    send_to_network(buf);
}
```

---

## 🟢 `Value` — Type Checkers

All type checkers return `bool` and **never throw**. They correctly reflect any in-place mutations.

```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({"n": 42, "s": "hi", "arr": [1,2], "obj": {}})");

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
auto root = qbuem::parse(doc, R"({"id":42,"score":9.87,"tag":"vip","flag":true})");

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
auto root = qbuem::parse(doc, R"({"store": {"books": [{"title": "C++ Primer"}]}})");

// Runtime JSON Pointer (string)
std::string t = root.at("/store/books/0/title").as<std::string>();
// "C++ Primer"

// Compile-time JSON Pointer (zero runtime overhead)
using namespace qbuem::literals;
auto title = root.at("/store/books/0/title"_jptr).as<std::string>();
```

---

## 🟢 `Value` — Array Navigation

### `operator[](index)` — Index Access

Accepts `int`, `unsigned int`, or `size_t` — no cast needed. Returns an invalid `Value{}` if the index is out of range or negative (never throws).

```cpp
auto root = qbuem::parse(doc, R"({"tags": ["cpp", "simd", "hft"]})");
std::string first  = root["tags"][0].as<std::string>(); // "cpp"
std::string second = root["tags"][1].as<std::string>(); // "simd"
std::string third  = root["tags"][2].as<std::string>(); // "hft"

int i = 1;
std::string by_var = root["tags"][i].as<std::string>(); // "simd" — int var works too
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

Iterates all key-value pairs, including keys added via `insert()` after parsing.

```cpp
auto root = qbuem::parse(doc, R"({"a": 1, "b": 2})");
root.insert("c", 3); // structural addition

for (auto [key, val] : root.items()) {
    std::cout << key << " = " << val.as<int>() << "\n";
    // outputs: a=1, b=2, c=3  (insert()ed key is included)
}

// Keys and values separately
for (std::string_view key : root.keys()) { /* ... */ }
for (auto val : root.values()) { /* ... */ }
```

### `.elements()` — Array Elements

```cpp
auto root = qbuem::parse(doc, R"({"scores": [95, 87, 100]})");

for (auto elem : root["scores"].elements()) {
    std::cout << elem.as<int>() << " ";  // 95 87 100
}
```

### `.as_array<T>()` — Typed Array View (Zero Allocation)

Iterates the array, casting each element to `T`. **Throws `std::runtime_error`** if an element cannot be cast to `T`. Use when you know all elements have a uniform type.

```cpp
for (int score : root["scores"].as_array<int>()) {
    std::cout << score << "\n";
}
```

### `.try_as_array<T>()` — Optional Typed Array (Non-Throwing)

Returns `std::optional<T>` per element — **never throws**. Use when the array may contain mixed types or elements that might not match `T`.

```cpp
for (auto v : root["scores"].try_as_array<int>()) {
    if (v) std::cout << *v << "\n";  // skip elements that can't be cast to int
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

## 🟡 Serialization — `dump()` vs `write()`

> **Quick rule:** use `.dump()` on a **parsed** `Value`; use `qbuem::write()` for **C++ objects**.
>
> | You have… | Use… |
> | :--- | :--- |
> | `qbuem::Value` from `qbuem::parse()` | `value.dump()` |
> | C++ struct / STL container / scalar | `qbuem::write(obj)` |

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

qbuem-json uses **non-destructive mutations**: the original tape is immutable. Changes live in a fast overlay map.

### Scalar Mutation — `set(v)` / `operator=(v)`

```cpp
auto root = qbuem::parse(doc, R"({"id": 1, "name": "Alice", "score": 87.5})");

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

Reverts a scalar mutation back to the **original parsed value**. This is not a "reset to null" — after `unset()`, the value behaves exactly as if it had never been mutated.

```cpp
root["id"].unset();
std::cout << root["id"].as<int>(); // 1 (original restored, NOT null)
// type_name() also returns the original type ("int", not "null")
```

> [!NOTE]
> `unset()` only removes the scalar mutation overlay. Keys added via `insert()` and array elements added via `push_back()` are **not** affected by `unset()` — those are structural mutations managed separately.

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

Accepts `int`, `unsigned int`, or `size_t` for array indices — no cast needed.

```cpp
root.erase("deprecated");   // remove object key
root["tags"].erase(0);      // remove first array element
root["tags"].erase(1);      // remove by plain int — works fine

int i = 2;
root["tags"].erase(i);      // int variable — also works
```

### `.push_back(value)` — Append to Array

Appended elements are immediately visible via `size()`, `elements()`, and `items()`.

```cpp
root["tags"].push_back(std::string_view{"new_tag"});
root["tags"].push_back(true);
root["tags"].push_back(42);
root["tags"].push_back(nullptr);

// size() reflects push_back() additions:
size_t n = root["tags"].size(); // tape count + push_back count
```

### `.push_back_json(raw_json)` — Append from Raw JSON

```cpp
root["users"].push_back_json(R"({"id": 3, "name": "Charlie"})");
root["ids"].push_back_json("99");
```

### `.merge(other)` / `.merge_patch(json)` — RFC 7396 Merge Patch

```cpp
auto root = qbuem::parse(doc, R"({"a": 1, "b": 2, "c": 3})");

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

## 🟣 `QBUEM_JSON_FIELDS` Macro

Auto-generates `from_qbuem_json` / `to_qbuem_json` **free functions** at compile time via Argument-Dependent Lookup (ADL). It **must** be placed **outside** the struct definition.

::: warning Why outside?
`QBUEM_JSON_FIELDS` expands to free function definitions. If placed inside a struct body, they become **member functions** instead, and ADL cannot find them — `qbuem::read<T>()` and `qbuem::write()` will fail to compile with a "no matching function" error. Always place the macro at **namespace scope**, immediately after the closing `}` of the struct.
:::

```cpp
struct Address {
    std::string street;
    std::string city;
    std::string country;
};
QBUEM_JSON_FIELDS(Address, street, city, country)   // ← outside, at namespace scope

struct User {
    uint64_t    id;
    std::string username;
    Address     address;                              // nested — auto-recursive
    std::vector<std::string> tags;                   // STL — auto
    std::optional<double>    score;                  // null when empty
    bool        active = true;
};
QBUEM_JSON_FIELDS(User, id, username, address, tags, score, active) // ← outside

// ❌ WRONG — placing it inside the struct makes from_qbuem_json a member function
// struct Bad {
//     int x;
//     QBUEM_JSON_FIELDS(Bad, x)   // inside struct → ADL breaks → compile error
// };

// Deserialize
auto user = qbuem::read<User>(R"({
    "id": 1, "username": "alice",
    "address": {"street": "Main St", "city": "Seoul", "country": "KR"},
    "tags": ["admin"], "score": 99.5, "active": true
})");

// Serialize
std::string json = qbuem::write(user);
std::string pretty = qbuem::write(user, 2); // pretty print
```

**Rules:**
- Missing JSON fields keep their C++ **default values** (not an error)
- JSON `null` on a non-optional field is **silently skipped**
- Extra JSON fields not in the struct are **ignored**
- Supports up to **32 fields** per struct (see [Large Structs](#large-structs-32-fields) below)
- Works **recursively** for nested structs

---

## 🟣 Large Structs (> 32 fields) {#large-structs-32-fields}

`QBUEM_JSON_FIELDS` is a variadic macro and supports a maximum of **32 fields**. For structs with more fields, define `from_qbuem_json` / `to_qbuem_json` / `append_qbuem_json` free functions manually in the **same namespace** as the struct. This is the same ADL hook mechanism that the macro generates — just written by hand.

```cpp
struct BigOrder {
    uint64_t    id;
    std::string symbol;
    double      price;
    double      qty;
    double      filled_qty;
    int         side;          // 0=buy, 1=sell
    int         type;          // 0=limit, 1=market
    int         status;
    std::string client_id;
    std::string exchange_id;
    int64_t     created_at;
    int64_t     updated_at;
    double      fee;
    std::string fee_asset;
    bool        reduce_only;
    bool        post_only;
    std::string time_in_force; // 33rd field — macro limit exceeded
};

inline void from_qbuem_json(const qbuem::json::Value& v, BigOrder& o) {
    qbuem::json::detail::from_json_field(v, "id",           o.id);
    qbuem::json::detail::from_json_field(v, "symbol",       o.symbol);
    qbuem::json::detail::from_json_field(v, "price",        o.price);
    qbuem::json::detail::from_json_field(v, "qty",          o.qty);
    qbuem::json::detail::from_json_field(v, "filled_qty",   o.filled_qty);
    qbuem::json::detail::from_json_field(v, "side",         o.side);
    qbuem::json::detail::from_json_field(v, "type",         o.type);
    qbuem::json::detail::from_json_field(v, "status",       o.status);
    qbuem::json::detail::from_json_field(v, "client_id",    o.client_id);
    qbuem::json::detail::from_json_field(v, "exchange_id",  o.exchange_id);
    qbuem::json::detail::from_json_field(v, "created_at",   o.created_at);
    qbuem::json::detail::from_json_field(v, "updated_at",   o.updated_at);
    qbuem::json::detail::from_json_field(v, "fee",          o.fee);
    qbuem::json::detail::from_json_field(v, "fee_asset",    o.fee_asset);
    qbuem::json::detail::from_json_field(v, "reduce_only",  o.reduce_only);
    qbuem::json::detail::from_json_field(v, "post_only",    o.post_only);
    qbuem::json::detail::from_json_field(v, "time_in_force",o.time_in_force);
}

inline void to_qbuem_json(qbuem::json::Value& v, const BigOrder& o) {
    qbuem::json::detail::to_json_field(v, "id",           o.id);
    qbuem::json::detail::to_json_field(v, "symbol",       o.symbol);
    qbuem::json::detail::to_json_field(v, "price",        o.price);
    qbuem::json::detail::to_json_field(v, "qty",          o.qty);
    qbuem::json::detail::to_json_field(v, "filled_qty",   o.filled_qty);
    qbuem::json::detail::to_json_field(v, "side",         o.side);
    qbuem::json::detail::to_json_field(v, "type",         o.type);
    qbuem::json::detail::to_json_field(v, "status",       o.status);
    qbuem::json::detail::to_json_field(v, "client_id",    o.client_id);
    qbuem::json::detail::to_json_field(v, "exchange_id",  o.exchange_id);
    qbuem::json::detail::to_json_field(v, "created_at",   o.created_at);
    qbuem::json::detail::to_json_field(v, "updated_at",   o.updated_at);
    qbuem::json::detail::to_json_field(v, "fee",          o.fee);
    qbuem::json::detail::to_json_field(v, "fee_asset",    o.fee_asset);
    qbuem::json::detail::to_json_field(v, "reduce_only",  o.reduce_only);
    qbuem::json::detail::to_json_field(v, "post_only",    o.post_only);
    qbuem::json::detail::to_json_field(v, "time_in_force",o.time_in_force);
}

inline void append_qbuem_json(std::string& out, const BigOrder& o) {
    out += '{';
    size_t prev = out.size();
    qbuem::json::detail::append_json(out, "id",           o.id);
    qbuem::json::detail::append_json(out, "symbol",       o.symbol);
    // ... repeat for all fields ...
    qbuem::json::detail::append_json(out, "time_in_force",o.time_in_force);
    if (out.size() > prev) out.pop_back(); // remove trailing comma
    out += '}';
}
```

**Alternative: split into sub-structs**

If logically possible, decompose a large struct into smaller nested ones — each with ≤ 32 fields — and use `QBUEM_JSON_FIELDS` on all of them:

```cpp
struct OrderCore   { uint64_t id; std::string symbol; double price; double qty; /* ... */ };
struct OrderFlags  { bool reduce_only; bool post_only; std::string time_in_force; /* ... */ };
struct BigOrder    { OrderCore core; OrderFlags flags; /* ... */ };

QBUEM_JSON_FIELDS(OrderCore,  id, symbol, price, qty, ...)
QBUEM_JSON_FIELDS(OrderFlags, reduce_only, post_only, time_in_force, ...)
QBUEM_JSON_FIELDS(BigOrder,   core, flags)
```

> **Note:** The sub-struct approach changes the JSON shape (fields are now nested under `"core"` / `"flags"`). Use manual ADL hooks when you need to preserve a flat JSON layout.

---

## 🟣 ADL Hooks for Third-Party Types

If you cannot modify a struct (e.g., from an external library), define these two functions in the **same namespace** as the type:

```cpp
namespace glm {
    // Teach qbuem-json to parse glm::vec3 from a JSON array
    inline void from_qbuem_json(const qbuem::Value& v, vec3& out) {
        out.x = v[0].as<float>();
        out.y = v[1].as<float>();
        out.z = v[2].as<float>();
    }

    // Teach qbuem-json to serialize glm::vec3 to a JSON array
    inline void to_qbuem_json(qbuem::Value& root, const vec3& in) {
        root = qbuem::Value::array();
        root.push_back(in.x);
        root.push_back(in.y);
        root.push_back(in.z);
    }
}

// Now glm::vec3 works natively!
glm::vec3 pos = qbuem::read<glm::vec3>("[1.0, 2.0, 3.5]");
std::string json = qbuem::write(pos); // "[1.0,2.0,3.5]"
```

---

## ⚡ Quick Reference Table

| API | Throws? | Returns | Use When |
| :--- | :---: | :--- | :--- |
| `qbuem::parse(doc, s)` | ❌ | `Value` | Standard lenient parsing |
| `qbuem::parse_strict(doc, s)` | ✅ | `Value` | Need RFC 8259 compliance |
| `qbuem::rfc8259::validate(s)` | ✅ | `void` | Validation without DOM |
| `qbuem::read<T>(s)` | ✅ | `T` | Deserialize directly |
| `qbuem::write(v)` | ❌ | `std::string` | Serialize any type |
| `qbuem::write(v, indent)` | ❌ | `std::string` | Pretty print |
| `qbuem::write_to(buf, v)` | ❌ | `void` | Zero-alloc hot loops |
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
