# Error Handling

qbuem-json gives you full control over how errors are handled. You can choose between **throwing exceptions** for known schemas, **monadic chaining** for untrusted data, or **explicit boolean checks** for fine-grained control.

## 🗺️ Choosing Your Strategy

| Strategy | API | Throws? | Best For |
| :--- | :--- | :---: | :--- |
| **Exception-based** | `as<T>()` | ✅ Yes | Known schemas, RPC calls, internal APIs |
| **Monadic/Safe** | `.get("key")` | ❌ No | API responses, config files, user input |
| **Boolean Check** | `is_int()`, `is_valid()` | ❌ No | Fine-grained validation, diagnostics |
| **Pipe Fallback** | `\| default` | ❌ No | Optional fields with sensible defaults |

---

## 💥 Strategy 1: Exception-Based (Strict)

The `as<T>()` API throws a `std::runtime_error` if the key is missing or the type doesn't match. Use it when you trust your data.

```cpp
try {
    qbuem::Document doc;
    auto root = qbuem::parse(doc, R"({"user": {"id": 42, "name": "Alice"}})");

    // Throws if "user" or "id" is missing, or "id" is not an int
    int id = root["user"]["id"].as<int>();
    std::string name = root["user"]["name"].as<std::string>();

    std::cout << "User #" << id << ": " << name << "\n";

} catch (const std::runtime_error& e) {
    std::cerr << "JSON error: " << e.what() << "\n";
}
```

### Parse Errors

A parse failure also throws `std::runtime_error`:

```cpp
try {
    qbuem::Document doc;
    auto root = qbuem::parse(doc, "{malformed json!}"); // throws
} catch (const std::runtime_error& e) {
    std::cerr << "Parse failed: " << e.what() << "\n";
}
```

---

## 🛡️ Strategy 2: Monadic / Safe Chain (No Throw)

The `.get("key")` API returns a `SafeValue` — a proxy that propagates `absent` state through the entire chain. **It never throws.** This is ideal for deeply nested structures from untrusted sources.

```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({
    "config": { "server": { "timeout_ms": 5000 } }
})");

// Deep chain — safe even if any key is missing
int timeout = root.get("config").get("server").get("timeout_ms").value_or(3000);
// → 5000 (or 3000 if any key was missing)

// Check if a value exists before using it
auto city = root.get("user").get("address").get("city");
if (city) { // evaluates to bool
    std::cout << "City: " << city.value_or("") << "\n";
}
```

### `value_or(default)` — Safe Extraction

```cpp
std::string mode    = root.get("settings").get("mode").value_or(std::string{"auto"});
int         timeout = root.get("settings").get("timeout").value_or(5000);
bool        debug   = root.get("settings").get("debug").value_or(false);
```

### Pipe Syntax

The `|` operator is shorthand for `.value_or()` on a `SafeValue`:

```cpp
int port = root.get("server").get("port") | 8080; // 8080 if missing
```

### `size()` and `empty()` — Safe Container Inspection

`SafeValue` has its own `.size()` and `.empty()` that never throw — they return `0` / `true` when the value is absent.

```cpp
// ❌ THROWS bad_optional_access when "items" is missing
size_t n = root.get("items")->size();

// ✅ Returns 0 safely — never throws
size_t n = root.get("items").size();
bool   b = root.get("items").empty();
```

Always call `.size()` and `.empty()` directly on the `SafeValue` (without `->`) to stay in the no-throw zone.

---

## 🔍 Strategy 3: Boolean Type Checks (Explicit Validation)

For diagnostic-quality error messages or when you need to distinguish "missing" from "wrong type":

```cpp
auto v = root["user_count"];

if (!v.is_valid()) {
    log_error("'user_count' key is missing from JSON");
} else if (v.is_int()) {
    process_count(v.as<int>());
} else {
    log_error("'user_count' expected int, got: " + std::string(v.type_name()));
}
```

#### Full list of boolean checkers:

```cpp
v.is_valid();    // key exists
v.is_null();     // null
v.is_bool();     // true / false
v.is_int();      // integer number (int64_t, uint64_t, etc.)
v.is_double();   // floating-point number
v.is_number();   // is_int() || is_double()
v.is_string();   // "text"
v.is_array();    // [...]
v.is_object();   // {...}

v.type_name();   // "null", "bool", "int", "double", "string", "array", "object"
```

---

## 🔐 Strategy 4: RFC 8259 Strict Validation

Use `qbuem::parse_strict()` or `qbuem::rfc8259::validate()` when you need to enforce strict JSON compliance (e.g., for security-sensitive input processing).

```cpp
#include <qbuem_json/qbuem_json.hpp>

// Validate without parsing (just check validity)
try {
    qbuem::rfc8259::validate("[1, 2,]");  // trailing comma → throws
} catch (const std::runtime_error& e) {
    std::cerr << e.what(); // "RFC 8259 violation at offset 7: trailing comma"
}

// Validate and parse in one step
try {
    qbuem::Document doc;
    auto root = qbuem::parse_strict(doc, R"({"key": "value"})"); // OK
    auto bad  = qbuem::parse_strict(doc, R"({"a": 01})");        // leading zero → throws
} catch (const std::runtime_error& e) {
    std::cerr << "Strict parse failed: " << e.what() << "\n";
}
```

**Inputs rejected by strict mode:**

| Input | Reason |
| :--- | :--- |
| `[1, 2,]` | Trailing comma |
| `{"a": 01}` | Leading zero |
| `{"key": 'val'}` | Single-quoted string |
| `{"\\uD800": "x"}` | Lone surrogate |
| `[1, 2} garbage` | Trailing garbage |
| `` (empty) | Empty input |

---

## 💡 Strategy Comparison: Real-World Example

Consider parsing a user profile from an API (which may be incomplete):

```cpp
struct UserView {
    std::string name;
    std::string email;
    std::string city;
    int         age;
    bool        is_admin;
};

UserView build_view(std::string_view json) {
    qbuem::Document doc;
    auto root = qbuem::parse(doc, json);

    return UserView {
        // Required fields → throw if missing (programmer error)
        .name  = root["name"].as<std::string>(),
        .email = root["email"].as<std::string>(),

        // Optional fields → safe monadic chain with defaults
        .city     = root.get("address").get("city").value_or(std::string{"Unknown"}),
        .age      = root["age"] | 0,
        .is_admin = root["roles"].get("admin") | false,
    };
}
```
