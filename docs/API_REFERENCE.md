# Beast JSON — API Reference

> Version: v1.0 (ctest 368/368 PASS)
> C++ Standard: C++20
> Header: `#include <beast_json/beast_json.hpp>`

---

## Table of Contents

1. [Namespaces](#namespaces)
2. [beast::Document](#beastdocument)
3. [beast::Value](#beastvalue)
   - [Type Checking](#type-checking)
   - [Typed Access — Throwing](#typed-access--throwing)
   - [Typed Access — Non-Throwing](#typed-access--non-throwing)
   - [Implicit Conversion](#implicit-conversion)
   - [Pipe Fallback Operator](#pipe-fallback-operator)
   - [Container Navigation](#container-navigation)
   - [Structural Mutation](#structural-mutation)
   - [Value Mutation](#value-mutation)
   - [Iteration](#iteration)
   - [Serialization](#serialization)
   - [Convenience](#convenience)
   - [JSON Pointer](#json-pointer)
   - [Merge / Merge Patch](#merge--merge-patch)
4. [beast::SafeValue](#beastsafevalue)
5. [Free Functions](#free-functions)
   - [parse / parse\_strict](#parse--parse_strict)
   - [read / write](#read--write)
6. [Auto-Serialization Macro](#auto-serialization-macro)
7. [RFC 8259 Validator](#rfc-8259-validator)
8. [C API (bindings/c)](#c-api-bindingsc)
9. [Python Binding (bindings/python)](#python-binding-bindingspython)

---

## Namespaces

| Namespace | Contents |
|:---|:---|
| `beast` | Public facade: `Document`, `Value`, `SafeValue`, `parse()`, `read<T>()`, `write()`, `parse_strict()` |
| `beast::rfc8259` | RFC 8259 strict validator: `validate()` |
| `beast::core` | Internal engine types: `TapeNode`, `TapeArena`, `Stage1Index`, `Parser` |
| `beast::utils` | Platform macros: `BEAST_INLINE`, `BEAST_HAS_AVX512`, `BEAST_ARCH_*` |

---

## beast::Document

A `Document` owns the tape arena and holds a reference to the JSON source. Parse into a `Document`, then navigate via the returned `Value`.

```cpp
class Document { /* opaque */ };
```

**Usage:**

```cpp
beast::Document doc;
beast::Value root = beast::parse(doc, json_string_view);
```

- A single `Document` may be reused across multiple calls to `beast::parse()` (tape is reset, overlay maps cleared).
- The `Document` must outlive all `Value` objects derived from it.
- `Document` is move-constructible/assignable; **not** copyable.

---

## beast::Value

The primary accessor type. `Value` is a lightweight (16-byte) handle: a pointer to the owning `Document` plus a tape index. It provides zero-copy, on-demand access to parsed JSON.

An invalid (null) `Value{}` is returned by any access that fails (missing key, out-of-range index, type mismatch on non-throwing paths). Use `operator bool()` or `is_valid()` to check validity.

---

### Type Checking

All `is_*()` methods are `const noexcept` and return `bool`.

| Method | Returns `true` when value is… |
|:---|:---|
| `is_null()` | JSON `null` |
| `is_bool()` | `true` or `false` |
| `is_int()` | Integer number |
| `is_double()` | Floating-point number |
| `is_number()` | Any number (integer, double, or raw) |
| `is_string()` | JSON string |
| `is_object()` | JSON object `{}` |
| `is_array()` | JSON array `[]` |
| `is_valid()` | Non-null handle (points to a real node) |
| `operator bool()` | Same as `is_valid()` |
| `type_name()` | Returns `std::string_view`: `"null"`, `"bool"`, `"int"`, `"double"`, `"string"`, `"array"`, `"object"`, `"invalid"` |

---

### Typed Access — Throwing

```cpp
template <typename T>
T as() const;
```

Extracts the value as type `T`. Throws `std::runtime_error` on type mismatch or if the value is invalid.

**Supported types for `T`:**

| `T` | JSON type required |
|:---|:---|
| `bool` | boolean |
| `int`, `int32_t`, `int64_t`, `long`, `long long` | integer |
| `uint32_t`, `uint64_t`, `unsigned long` | integer (non-negative) |
| `float`, `double` | number (integer or double) |
| `std::string` | string (unescaped copy) |
| `std::string_view` | string (zero-copy view into source) |

```cpp
int id       = root["user"]["id"].as<int>();
double score = root["score"].as<double>();
std::string name = root["name"].as<std::string>();
std::string_view sv = root["name"].as<std::string_view>(); // zero-copy
```

---

### Typed Access — Non-Throwing

```cpp
template <typename T>
std::optional<T> try_as() const noexcept;
```

Returns `std::optional<T>`. Returns `std::nullopt` on type mismatch or invalid value. Never throws.

```cpp
auto id = root["user"]["id"].try_as<int>();   // std::optional<int>
if (id) std::cout << *id << "\n";
```

---

### Implicit Conversion

`Value` provides `operator T()` implicit conversions (same set as `as<T>()`):

```cpp
int    age  = root["age"];              // implicit int
double sc   = root["score"];            // implicit double
std::string name = root["name"];        // implicit std::string
bool   flag = root["active"];           // implicit bool
```

Throws `std::runtime_error` on type mismatch (same as `as<T>()`).

---

### Pipe Fallback Operator

```cpp
template <typename T>
T operator|(T default_value) const noexcept;
```

Returns the value as `T`, or `default_value` if the value is invalid, missing, or the wrong type. **Never throws.**

```cpp
int  age  = root["age"]  | 18;           // 18 if missing or wrong type
std::string s = root["name"] | std::string{"anonymous"};
double v  = root["score"] | 0.0;
```

---

### Container Navigation

#### Object access

```cpp
Value operator[](std::string_view key) const noexcept;
Value operator[](const char* key) const noexcept;
```

Returns the **value** for the given key. Returns an invalid `Value{}` if:
- The current value is not an object
- The key does not exist

```cpp
Value user = root["user"];
Value id   = root["user"]["id"];  // chain — safe even if "user" is missing
```

#### Array access

```cpp
Value operator[](size_t index) const noexcept;
Value operator[](int index) const noexcept;
```

Returns the element at `index`. Returns invalid `Value{}` if out of range or not an array.

```cpp
Value first = root["tags"][0];
Value third = root["tags"][2];
```

#### find()

```cpp
std::optional<Value> find(std::string_view key) const noexcept;
```

Returns `std::optional<Value>` — `std::nullopt` if the key is absent. Useful when you need to distinguish "key absent" from "key present with null value".

```cpp
if (auto v = root.find("optional_field")) {
    // key exists
    std::cout << v->as<int>() << "\n";
}
```

#### size() / empty()

```cpp
size_t size()  const noexcept;
bool   empty() const noexcept;
```

For arrays: number of elements. For objects: number of key-value pairs. Accounts for deleted overlay entries. Returns `0` for non-container types.

```cpp
std::cout << root["tags"].size() << "\n";   // 3
std::cout << root["tags"].empty() << "\n";  // false
```

#### contains()

```cpp
bool contains(std::string_view key) const noexcept;
```

Equivalent to `find(key).has_value()`. Sugar for safe key existence check.

```cpp
if (root.contains("config"))
    process(root["config"]);
```

#### value(key, default) / value(index, default)

```cpp
template <typename T>
T value(std::string_view key, T default_val) const noexcept;

template <typename T>
T value(size_t index, T default_val) const noexcept;
```

Returns `(*this)[key].as<T>()` if the key exists and the type matches; otherwise returns `default_val`. Never throws.

```cpp
int timeout = root.value("timeout", 5000);
std::string mode = root.value("mode", std::string{"default"});
```

---

### Structural Mutation

Overlay-based: the original tape is never modified. Mutations are recorded in per-`Document` overlay maps and reflected in `dump()`, `size()`, `find()`, `operator[]`, and iterators.

#### erase()

```cpp
bool erase(std::string_view key);  // object: remove key-value pair
bool erase(size_t index);          // array: remove element at index
```

Returns `true` if the key/index existed and was marked for deletion.

```cpp
root["tags"].erase(0);      // remove first tag
root.erase("deprecated");   // remove object key
```

#### insert() / insert\_json()

```cpp
template <typename T>
bool insert(std::string_view key, T value);     // object: add key with typed value
bool insert_json(std::string_view key, std::string raw_json);  // add pre-serialized JSON
```

Adds a new key-value pair to an object. If the key already exists in the original tape, both the original and new entry will appear (use `erase` first to replace).

```cpp
root.insert("version", 2);
root.insert("active", true);
root.insert("score", 3.14);
root.insert("label", std::string_view{"hello"});
root.insert_json("nested", R"({"x":1,"y":2})");
```

#### push\_back() / push\_back\_json()

```cpp
template <typename T>
bool push_back(T value);              // array: append typed value
bool push_back_json(std::string raw); // array: append pre-serialized JSON
```

```cpp
root["tags"].push_back(std::string_view{"new-tag"});
root["items"].push_back_json(R"({"id":99})");
```

---

### Value Mutation

Scalar overlay: replace the effective value of a node without altering the tape. Takes effect immediately in `dump()`, `as<T>()`, `try_as<T>()`, `is_*()`.

#### set() / operator=()

```cpp
void set(std::nullptr_t);
void set(bool v);
void set(int64_t v);
void set(double v);
void set(std::string_view v);

Value& operator=(std::nullptr_t);
Value& operator=(bool v);
Value& operator=(int64_t v);   // also matches int, long, etc.
Value& operator=(double v);
Value& operator=(std::string_view v);
Value& operator=(const char* v);
```

```cpp
root["user"]["id"] = 99;        // same as root["user"]["id"].set(99LL)
root["user"]["name"] = "Eve";
root["user"]["score"] = 9.9;
root["flag"] = true;
root["ptr"] = nullptr;
```

#### unset()

```cpp
void unset();
```

Removes the scalar overlay, restoring the original parsed value.

```cpp
root["user"]["id"].unset();   // restore parsed value
```

---

### Iteration

#### items() — object iteration

```cpp
using ObjectItem = std::pair<std::string_view, Value>;
ObjectRange items() const noexcept;
```

Returns a range of `ObjectItem = std::pair<std::string_view, Value>`. Supports range-for, structured bindings, and all `std::ranges` algorithms.

```cpp
for (auto [key, val] : root.items())
    std::cout << key << ": " << val.dump() << "\n";

// std::ranges
auto it = std::ranges::find_if(root.items(),
    [](Value::ObjectItem kv){ return kv.first == "id"; });
```

#### elements() — array iteration

```cpp
ArrayRange elements() const noexcept;
```

Returns a range of `Value`. Supports range-for, structured bindings, and `std::ranges` algorithms.

```cpp
for (auto elem : root["tags"].elements())
    std::cout << elem.as<std::string>() << "\n";

// std::views pipeline
auto big = root["scores"].elements()
    | std::views::filter([](auto v){ return v.as<int>() > 3; });
```

Both `ObjectRange` and `ArrayRange` model `std::ranges::borrowed_range` — safe to pass to range algorithms without dangling.

#### keys() / values()

```cpp
auto keys()   const noexcept;  // lazy range of std::string_view
auto values() const noexcept;  // lazy range of Value
```

```cpp
for (std::string_view k : root.keys())
    std::cout << k << "\n";

for (auto v : root.values())
    std::cout << v.dump() << "\n";
```

#### as\_array\<T\>() / try\_as\_array\<T\>()

```cpp
template <typename T>
auto as_array() const;           // lazy view: each element cast to T (throws on mismatch)
template <typename T>
auto try_as_array() const;       // lazy view: each element as std::optional<T>
```

Zero-allocation typed views over array elements.

```cpp
for (int id : root["ids"].as_array<int>())
    std::cout << id << " ";

for (auto v : root["ids"].try_as_array<int>())
    if (v) std::cout << *v << " ";
```

---

### Serialization

#### dump() — returns new string

```cpp
std::string dump() const;
std::string dump(int indent) const;  // pretty-print with indent spaces
```

```cpp
std::string json = root.dump();
std::string pretty = root.dump(2);   // 2-space indent
std::string subtree = root["user"].dump();  // subtree serialization
```

#### dump(out) — buffer reuse (Phase 73 optimization)

```cpp
void dump(std::string& out) const;
```

Writes into the supplied `out` buffer. Reuses existing capacity — if the serialized size is unchanged from the prior call, the `resize()` is a no-op (zero-fill eliminated by the `last_dump_size_` cache, Phase 75B).

Use this overload for hot loops or repeated serialization of the same document.

```cpp
std::string buf;
while (true) {
    // update doc...
    root.dump(buf);   // ~O(1) on repeated identical documents
    send(buf);
}
```

---

### Convenience

| Method | Description |
|:---|:---|
| `contains(key)` | `bool` — key exists in object |
| `value(key, default)` | Safe extraction with default |
| `type_name()` | `std::string_view` type tag |
| `is_valid()` / `operator bool()` | Handle validity check |
| `size()` / `empty()` | Container size |

---

### JSON Pointer

#### Runtime JSON Pointer (RFC 6901)

```cpp
Value at(std::string_view path) const noexcept;
```

Navigates the document using a JSON Pointer path string. Returns invalid `Value{}` on any missing segment — never throws. Escape sequences `~0` (→ `~`) and `~1` (→ `/`) are handled.

```cpp
Value v = root.at("/users/0/name");
Value cfg = root.at("/config/timeout");
int n = root.at("/a/b/c").as<int>();
```

#### Compile-Time JSON Pointer

```cpp
template <beast::core::FixedString Path>
Value at() const noexcept;
```

The path is validated at compile time: must start with `/`, otherwise a `static_assert` fires. Zero runtime overhead beyond the equivalent chained `operator[]` calls.

```cpp
auto v = root.at<"/user/id">();
auto t = root.at<"/config/timeout">().as<int>();

// Compile error: path must start with '/'
// auto bad = root.at<"user/id">();
```

---

### Merge / Merge Patch

#### merge()

```cpp
bool merge(const Value& other) noexcept;
```

Copies all key-value pairs from `other` (must be an object) into the current object via the insertion overlay. Existing keys are not removed.

```cpp
beast::Document d2;
auto extra = beast::parse(d2, R"({"debug":true,"version":2})");
root.merge(extra);
```

#### merge\_patch() (RFC 7396)

```cpp
bool merge_patch(std::string_view patch_json);
```

Applies a JSON Merge Patch. Rules:
- `null` value → delete the key
- object value → recursive patch
- any other value → overwrite

```cpp
root.merge_patch(R"({"timeout":10000,"deprecated":null})");
// timeout overwritten, deprecated key deleted
```

---

## beast::SafeValue

An optional-propagating proxy. Obtained via `Value::get()`. All chained accesses on a `SafeValue` return another `SafeValue`, propagating absence silently — no exceptions, no crashes.

```cpp
beast::SafeValue sv = root.get("user");     // SafeValue (may be absent)
beast::SafeValue id = sv["id"];             // propagates absence
std::optional<int> opt = id.as<int>();      // std::optional<int>
int safe = sv["timeout"].value_or(5000);    // default if any step absent
```

**SafeValue API:**

| Method | Description |
|:---|:---|
| `operator[](key)` / `operator[](idx)` | Propagating navigation |
| `as<T>()` | `std::optional<T>` — nullopt if absent |
| `value_or(T)` | Returns value or default |
| `has_value()` / `operator bool()` | Check if present |
| `operator*()` / `operator->()` | Dereference to `Value` (UB if absent) |
| `operator\|` | Same as `value_or` via pipe syntax |

```cpp
// Deep safe chain — absent at any level → 0
int depth = root.get("a")["b"]["c"]["d"].value_or(0);

// Never throws, never crashes
auto maybe_name = root.get("user")["profile"]["name"].as<std::string>();
// → std::optional<std::string>
```

---

## Free Functions

### parse / parse\_strict

```cpp
namespace beast {

// Standard parse (non-strict, best-effort)
Value parse(Document& doc, std::string_view json);

// RFC 8259 strict parse — throws on any violation
Value parse_strict(Document& doc, std::string_view json);

}
```

`parse_strict` internally calls `beast::rfc8259::validate(json)` before parsing. On violation, throws `std::runtime_error` with message format:
```
JSON parse error: RFC 8259 violation at offset N: <reason>
```

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"key": "value"})");

// Strict — rejects trailing commas, leading zeros, lone surrogates, etc.
try {
    auto strict = beast::parse_strict(doc, "[1,2,]");
} catch (const std::runtime_error& e) {
    std::cerr << e.what();  // "...trailing comma at offset 5"
}
```

### read / write

```cpp
namespace beast {

// Deserialize JSON string → T
template <typename T>
T read(std::string_view json);

// Serialize T → JSON string
template <typename T>
std::string write(const T& value);

// Partial helpers
template <typename T>
void from_json(const Value& v, T& out);

template <typename T>
std::string to_json_str(const T& val);

}
```

Supported types `T` for `read<T>` / `write`:

| Category | Types |
|:---|:---|
| Primitives | `bool`, `int`, `long`, `long long`, `unsigned`, `uint64_t`, `float`, `double` |
| Strings | `std::string`, `std::string_view`, `const char*` |
| Optional | `std::optional<T>` ↔ `null` / T |
| Null | `std::nullptr_t` → `null` |
| Sequence | `std::vector<T>`, `std::list<T>`, `std::deque<T>` |
| Set | `std::set<T>`, `std::unordered_set<T>` |
| Map | `std::map<std::string, V>`, `std::unordered_map<std::string, V>` |
| Fixed array | `std::array<T, N>` |
| Tuple/Pair | `std::tuple<Ts...>`, `std::pair<A, B>` |
| Custom struct | `BEAST_JSON_FIELDS(Type, field…)` |
| ADL | `from_beast_json(Value, T&)` / `to_beast_json(Value&, T)` |

Unsupported types produce a `static_assert` at compile time with a clear message.

```cpp
// STL types — zero effort
auto v = beast::read<std::vector<int>>("[1,2,3]");
auto m = beast::read<std::map<std::string,double>>(R"({"pi":3.14})");
auto t = beast::read<std::tuple<int,std::string,bool>>("[42,\"ok\",true]");

// Write
std::string j1 = beast::write(std::pair{3, "hello"s});   // [3,"hello"]
std::string j2 = beast::write(std::array<int,3>{1,2,3}); // [1,2,3]
std::string j3 = beast::write(std::optional<int>{});      // null
```

---

## Auto-Serialization Macro

```cpp
BEAST_JSON_FIELDS(StructType, field1, field2, ...)
```

Registers `StructType` for automatic bidirectional JSON serialization. Place after the struct definition (in the same namespace or globally). Supports up to 32 fields. Works with nested structs, STL containers, and `std::optional` fields — all recursively automatic.

```cpp
struct Address {
    std::string city;
    std::string country;
};
BEAST_JSON_FIELDS(Address, city, country)

struct User {
    std::string              name;
    int                      age    = 0;
    Address                  addr;                    // nested — auto
    std::vector<std::string> tags;                    // STL — auto
    std::optional<double>    score;                   // optional — auto
};
BEAST_JSON_FIELDS(User, name, age, addr, tags, score)

// Read
auto user = beast::read<User>(json_string);

// Write
std::string json = beast::write(user);
```

**Behavior on missing/null JSON fields:**
- Missing field → struct member keeps its default value (no crash)
- JSON `null` on non-optional field → skip (no crash)
- Extra JSON fields not in the struct → silently ignored

**ADL extension point:**

```cpp
// Custom: implement these two free functions in the same namespace as T
void from_beast_json(const beast::Value& v, MyType& out);
void to_beast_json(beast::Value& v, const MyType& in);
```

---

## RFC 8259 Validator

```cpp
namespace beast::rfc8259 {

void validate(std::string_view json);  // throws std::runtime_error on violation

}
```

Standalone validator — does not allocate, does not modify any state. Throws `std::runtime_error` with an offset-annotated message on any RFC 8259 violation.

**Rejection criteria (56 test cases):**

| Violation | Example |
|:---|:---|
| Trailing comma | `[1,2,]` · `{"a":1,}` |
| Leading zero | `01` · `007` |
| Bare decimal | `.5` · `1.` |
| Incomplete escape | `"\u00"` · `"\x41"` |
| Control character in string | `"\t"` (literal tab, unescaped) |
| Lone surrogate | `"\uD800"` |
| Trailing garbage | `{}extra` |
| Empty input | `` (empty string) |
| Truncated input | `[1,2` · `{"a":` |
| Wrong type at root | (bare number, string, etc. are valid by RFC 8259 §2) |

```cpp
try {
    beast::rfc8259::validate("[1,2,]");
} catch (const std::runtime_error& e) {
    std::cout << e.what();
    // → "RFC 8259 violation at offset 5: trailing comma"
}

// Validate + parse in one call
beast::Document doc;
auto root = beast::parse_strict(doc, json_str);
```

---

## C API (bindings/c)

Build: `cmake -DBEAST_JSON_BUILD_BINDINGS=ON`.
Output: `build/bindings/c/libbeast_json_c.so` (Linux) / `.dylib` (macOS) / `.dll` (Windows).
Header: `bindings/c/beast_json_c.h`.

### Types

```c
typedef struct BJSONDocument BJSONDocument;  // opaque document handle
typedef struct BJSONValue    BJSONValue;     // opaque value handle (non-owning)
typedef struct BJSONIter     BJSONIter;      // opaque iterator handle
```

### Document lifecycle

```c
BJSONDocument* bjson_doc_create(void);
void           bjson_doc_destroy(BJSONDocument* doc);
```

### Parsing

```c
BJSONValue* bjson_parse(BJSONDocument* doc, const char* json, size_t len);
BJSONValue* bjson_parse_strict(BJSONDocument* doc, const char* json, size_t len);
const char* bjson_last_error(BJSONDocument* doc);  // NULL on success
```

### Type inspection

```c
int         bjson_type(const BJSONValue* v);          // BJSON_TYPE_* enum
const char* bjson_type_name(const BJSONValue* v);     // "null", "int", etc.
int         bjson_is_valid(const BJSONValue* v);       // 1 if valid
```

`BJSON_TYPE_*` constants: `BJSON_TYPE_NULL`, `BJSON_TYPE_BOOL`, `BJSON_TYPE_INT`, `BJSON_TYPE_DOUBLE`, `BJSON_TYPE_STRING`, `BJSON_TYPE_ARRAY`, `BJSON_TYPE_OBJECT`.

### Scalar extraction

```c
int         bjson_as_bool(const BJSONValue* v, int* out);
int         bjson_as_int(const BJSONValue* v, int64_t* out);
int         bjson_as_double(const BJSONValue* v, double* out);
const char* bjson_as_string(const BJSONValue* v, size_t* len_out); // zero-copy
```

Returns 1 on success, 0 on type mismatch. `bjson_as_string` returns NULL on failure.

### Container navigation

```c
size_t       bjson_size(const BJSONValue* v);
int          bjson_empty(const BJSONValue* v);
BJSONValue*  bjson_get_idx(BJSONDocument* doc, const BJSONValue* v, size_t idx);
BJSONValue*  bjson_get_key(BJSONDocument* doc, const BJSONValue* v,
                           const char* key, size_t key_len);
BJSONValue*  bjson_at_path(BJSONDocument* doc, const BJSONValue* v,
                           const char* path);  // RFC 6901 JSON Pointer
```

### Iteration

```c
BJSONIter*  bjson_iter_create(BJSONDocument* doc, const BJSONValue* v);
int         bjson_iter_next(BJSONIter* it);
const char* bjson_iter_key(BJSONIter* it, size_t* key_len_out); // objects only
BJSONValue* bjson_iter_value(BJSONIter* it);
void        bjson_iter_destroy(BJSONIter* it);
```

### Serialization

```c
char* bjson_dump(BJSONDocument* doc, const BJSONValue* v, size_t* out_len);
char* bjson_dump_pretty(BJSONDocument* doc, const BJSONValue* v,
                        int indent, size_t* out_len);
// Caller must free() the returned string.
```

### Mutation

```c
int bjson_set_int(BJSONValue* v, int64_t val);
int bjson_set_double(BJSONValue* v, double val);
int bjson_set_string(BJSONValue* v, const char* s, size_t len);
int bjson_set_null(BJSONValue* v);
int bjson_set_bool(BJSONValue* v, int val);
int bjson_insert_raw(BJSONDocument* doc, BJSONValue* obj,
                     const char* key, size_t key_len,
                     const char* json, size_t json_len);
int bjson_erase_key(BJSONDocument* doc, BJSONValue* obj,
                    const char* key, size_t key_len);
int bjson_erase_idx(BJSONDocument* doc, BJSONValue* arr, size_t idx);
```

### Complete C example

```c
#include "beast_json_c.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    BJSONDocument* doc = bjson_doc_create();
    const char* json = "{\"name\":\"Alice\",\"score\":99}";
    BJSONValue* root = bjson_parse(doc, json, strlen(json));

    if (!bjson_is_valid(root)) {
        fprintf(stderr, "parse error: %s\n", bjson_last_error(doc));
        bjson_doc_destroy(doc);
        return 1;
    }

    // Read
    size_t name_len;
    const char* name = bjson_as_string(
        bjson_get_key(doc, root, "name", 4), &name_len);
    printf("name: %.*s\n", (int)name_len, name);  // Alice

    int64_t score;
    bjson_as_int(bjson_get_key(doc, root, "score", 5), &score);
    printf("score: %lld\n", (long long)score);  // 99

    // Mutate + serialize
    BJSONValue* score_v = bjson_get_key(doc, root, "score", 5);
    bjson_set_double(score_v, 100.0);

    size_t out_len;
    char* out = bjson_dump(doc, root, &out_len);
    printf("%.*s\n", (int)out_len, out);
    free(out);

    bjson_doc_destroy(doc);
    return 0;
}
```

---

## Python Binding (bindings/python)

Location: `bindings/python/beast_json.py`.
Requirements: CPython 3.8+, `ctypes` (stdlib). Build the C library first.

```bash
cmake -S . -B build -DBEAST_JSON_BUILD_BINDINGS=ON
cmake --build build --target beast_json_c
export BEAST_JSON_LIB_PATH=./build/bindings/c
```

### Classes

#### Document

```python
class Document:
    def __init__(self, json_str: str, strict: bool = False): ...
    def root(self) -> Value: ...
```

- `strict=True`: uses `bjson_parse_strict`; raises `ValueError` on RFC 8259 violation.
- `Document` owns memory; `Value` objects derived from it are valid while `Document` is alive.

#### Value

```python
class Value:
    # Navigation (returns Value — invalid if missing)
    def __getitem__(self, key_or_idx) -> Value: ...

    # RFC 6901 JSON Pointer
    def at(self, path: str) -> Value: ...

    # Type / validity
    @property
    def type_name(self) -> str: ...          # "null","int","bool","double","string","array","object","invalid"
    def is_valid(self) -> bool: ...
    def __bool__(self) -> bool: ...          # same as is_valid()

    # Native Python extraction
    def get(self) -> object: ...             # → dict / list / str / int / float / bool / None

    # Iteration
    def items(self): ...                     # yields (key: str, val: Value)
    def elements(self): ...                  # yields Value

    # Serialization
    def dump(self, indent: int = 0) -> str: ...

    # Mutation
    def set(self, value) -> None: ...
    def insert(self, key: str, value) -> None: ...
    def erase(self, key_or_idx) -> None: ...
```

#### Top-level functions

```python
def loads(json_str: str) -> object:
    """Drop-in replacement for json.loads(). Returns Python native objects."""

def dumps(obj: object) -> str:
    """Not yet implemented — use json.dumps() for Python→JSON."""
```

### Python examples

```python
from beast_json import Document, loads

# Basic parse & access
doc = Document('{"name":"Alice","tags":["go","cpp"],"addr":{"city":"Seoul"}}')
root = doc.root()

print(root["name"])           # Alice
print(root["addr"]["city"])   # Seoul
print(root["tags"][0])        # go
print(root["tags"].type_name) # array
print(root["missing"].is_valid())  # False

# JSON Pointer (RFC 6901)
print(root.at("/tags/1"))     # cpp

# Iteration
for key, val in root.items():
    print(f"{key}: {val.type_name}")

for tag in root["tags"].elements():
    print(tag)  # go, cpp

# Mutation
root["name"].set("Bob")
root.insert("version", 2)
root["tags"].erase(0)          # removes "go"
print(root.dump(indent=2))     # pretty JSON

# Python dict conversion
obj = root.get()               # → {'name': 'Bob', 'tags': ['cpp'], ...}

# RFC 8259 strict mode
try:
    bad = Document("[1,2,]", strict=True)
except ValueError as e:
    print(e)  # JSON parse error: RFC 8259 violation at offset 5: trailing comma

# Drop-in replacement for json.loads()
data = loads('[1, 2, {"x": 3}]')
print(data)   # [1, 2, {'x': 3}]
```
