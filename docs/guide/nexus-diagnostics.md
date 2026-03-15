# Nexus Diagnostics Guide

The Nexus engine (`fuse<T>()`, `QBUEM_JSON_FIELDS`) is designed for
zero-allocation, single-pass JSON-to-struct mapping.  Because performance is the
primary goal, type mismatches that occur at the field level are **silently
skipped** by default — the field keeps its default-initialised value and parsing
continues.

This guide explains how to surface those mismatches as readable errors during
development, how to interpret the messages, and how to switch back to the lenient
mode for production hot paths.

---

## Complete error message reference

Every `std::runtime_error` thrown by Nexus follows one of these templates.
Byte offsets are relative to the start of the JSON string passed to `fuse`.

| Situation | Example message |
|:---|:---|
| Root value is not an object | `Nexus: expected object, got array at byte offset 0` |
| Root value is a string | `Nexus: expected object, got string at byte offset 0` |
| Root value is a number | `Nexus: expected object, got number at byte offset 0` |
| Root value is `null` | `Nexus: expected object, got null at byte offset 0` |
| Root input is empty | `Nexus: expected object, got end-of-input at byte offset 0` |
| Field expects `bool`, got number | `Nexus: expected boolean, got number at byte offset 42` |
| Field expects `bool`, got string | `Nexus: expected boolean, got string at byte offset 42` |
| Field expects number, got string | `Nexus: expected number, got string at byte offset 42` |
| Field expects number, got `null` | `Nexus: expected number, got null at byte offset 42` |
| Field expects string, got object | `Nexus: expected string, got object at byte offset 42` |
| Field expects string, got array | `Nexus: expected string, got array at byte offset 42` |
| Field expects object (nested struct), got array | `Nexus: expected object, got array at byte offset 42` |
| Field expects array, got string | `Nexus: expected array, got string at byte offset 42` |
| Integer overflow or unexpected digits | `Nexus: integer parse failed (overflow or unexpected digits)` |
| Integer parse failed (with offset) | `Nexus: integer parse failed (overflow or unexpected digits) at byte offset 42` |

The token labels used in messages map one-to-one to the leading byte of each JSON value:

| Leading byte | Label in error |
|:---|:---|
| `{` | `object` |
| `[` | `array` |
| `"` | `string` |
| `t` / `f` | `boolean` |
| `n` | `null` |
| `-`, `0`–`9` | `number` |
| past end-of-input | `end-of-input` |
| anything else | `unknown token` |

---

## Enabling strict mode

### Option A — `fuse_strict<T>()` (runtime, per-call)

The easiest way to diagnose problems during development is to replace `fuse<T>()`
with `fuse_strict<T>()`:

```cpp
#include <qbuem_json/qbuem_json.hpp>

struct Order {
    uint64_t id;
    std::string symbol;
    double price;
    int qty;
};
QBUEM_JSON_FIELDS(Order, id, symbol, price, qty)

// lenient — silently skips type-mismatched fields:
Order a = qbuem::fuse<Order>(json);

// strict — throws std::runtime_error on any type mismatch:
Order b = qbuem::fuse_strict<Order>(json);
```

`fuse_strict` sets a thread-local flag before the call and clears it afterwards
(including on exception), so it is safe to use from multiple threads
simultaneously.  There is zero overhead on the `fuse<T>()` path.

### Option B — `QBUEM_NEXUS_STRICT` compile-time macro

Define `QBUEM_NEXUS_STRICT` before including the header to make **every** Nexus
call strict.  This is the recommended pattern for debug and test builds:

```cmake
# CMakeLists.txt — test target only
target_compile_definitions(my_tests PRIVATE QBUEM_NEXUS_STRICT)
```

```cpp
// Or in a single translation unit:
#define QBUEM_NEXUS_STRICT
#include <qbuem_json/qbuem_json.hpp>
```

When the macro is defined, `fuse_strict<T>()` and `fuse<T>()` behave
identically, so no source changes are needed between builds.

---

## Interpreting a type mismatch

### Example — scalar field

Suppose you have:

```cpp
struct Config {
    int timeout_ms;
    bool retry;
};
QBUEM_JSON_FIELDS(Config, timeout_ms, retry)
```

And the incoming JSON accidentally sends `timeout_ms` as a string:

```json
{"timeout_ms": "500", "retry": true}
```

With `fuse<Config>()`, `timeout_ms` silently stays `0`.  With
`fuse_strict<Config>()`:

```
terminate called after throwing an instance of 'std::runtime_error'
  what():  Nexus: expected number, got string at byte offset 16
```

Count from the start of the JSON to locate the offending token:

```
{"timeout_ms": "500", "retry": true}
0         1         2         3
0123456789012345678901234567890123456
               ^--- offset 16 = opening '"' of "500"
```

### Example — nested struct

```cpp
struct Address {
    std::string city;
    std::string country;
};
QBUEM_JSON_FIELDS(Address, city, country)

struct User {
    std::string name;
    Address address;
};
QBUEM_JSON_FIELDS(User, name, address)
```

If `address` is mistakenly sent as an array instead of an object:

```json
{"name": "Alice", "address": ["NYC", "US"]}
```

```
Nexus: expected object, got array at byte offset 23
```

Offset 23 points to the `[` that opens the array, which is exactly where the
nested struct expected a `{`.

### Example — optional field

`std::optional<T>` fields accept `null` without throwing, even in strict mode:

```cpp
struct Event {
    std::string name;
    std::optional<std::string> description;
};
QBUEM_JSON_FIELDS(Event, name, description)
```

```json
{"name": "Launch", "description": null}
```

`description` is set to `std::nullopt`.  No error is thrown because `null` is a
valid JSON representation of an absent optional.

If `description` receives the wrong non-null type in strict mode:

```json
{"name": "Launch", "description": 42}
```

```
Nexus: expected string, got number at byte offset 30
```

### Example — array of structs

```cpp
struct Trade {
    std::string symbol;
    double price;
    int qty;
};
QBUEM_JSON_FIELDS(Trade, symbol, price, qty)

struct Book {
    std::vector<Trade> trades;
};
QBUEM_JSON_FIELDS(Book, trades)
```

If one element inside the array has a wrong field type (strict mode):

```json
{"trades": [{"symbol": "AAPL", "price": "bad", "qty": 10}]}
```

```
Nexus: expected number, got string at byte offset 40
```

Offset 40 points directly to `"bad"` inside the nested object.

### Example — integer overflow

Large values that exceed a type's range are reported explicitly:

```cpp
struct Msg {
    uint8_t flags;
};
QBUEM_JSON_FIELDS(Msg, flags)
```

```json
{"flags": 9999}
```

```
Nexus: integer parse failed (overflow or unexpected digits) at byte offset 10
```

In lenient mode (`fuse<Msg>()`) the field silently keeps value `0`.

---

## Catching errors gracefully

All Nexus errors are `std::runtime_error`.  Wrap `fuse_strict` at a service
boundary:

```cpp
#include <stdexcept>
#include <qbuem_json/qbuem_json.hpp>

std::optional<Order> parse_order(std::string_view json) noexcept {
    try {
        return qbuem::fuse_strict<Order>(json);
    } catch (const std::runtime_error& e) {
        log_error("JSON mapping failed: {}", e.what());
        return std::nullopt;
    }
}
```

For non-throwing code paths, use lenient `fuse<T>()` with an explicit validation
step before the hot path:

```cpp
// In your request handler test suite
void validate_schema(std::string_view json) {
    qbuem::fuse_strict<Order>(json);  // throws on mismatch
}

// In production
Order o = qbuem::fuse<Order>(json);  // zero overhead
```

---

## Testing with strict mode (CI/CD integration)

### CMake — per-target

```cmake
# CMakeLists.txt
add_executable(unit_tests tests/main.cpp tests/order_tests.cpp)
target_link_libraries(unit_tests PRIVATE qbuem_json)

# Enable strict mode only for tests, never for production targets
target_compile_definitions(unit_tests PRIVATE QBUEM_NEXUS_STRICT)
```

### CMake — via a build preset

```json
// CMakePresets.json
{
  "configurePresets": [
    {
      "name": "debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_FLAGS": "-DQBUEM_NEXUS_STRICT"
      }
    }
  ]
}
```

### GitHub Actions

```yaml
# .github/workflows/ci.yml
- name: Configure (strict mode)
  run: cmake -B build -DCMAKE_BUILD_TYPE=Debug
             -DCMAKE_CXX_FLAGS="-DQBUEM_NEXUS_STRICT"

- name: Build and test
  run: cmake --build build && ctest --build-directory build --output-on-failure
```

---

## DOM type mismatch errors (`Value::as<T>()`)

Nexus diagnostics apply only to the zero-tape path.  When using the DOM API,
type mismatches are reported by `Value::as<T>()`:

```cpp
qbuem::Document doc = qbuem::parse(json);
qbuem::Value root = doc.root();

// throws: "qbuem::Value::as<bool>: not a boolean"
bool v = root["active"].as<bool>();

// non-throwing variant — returns std::nullopt on mismatch:
std::optional<bool> ov = root["active"].try_as<bool>();

// explicit check before conversion:
if (root["active"].is_bool()) {
    bool v2 = root["active"].as<bool>();
}
```

See [errors.md](errors.md) for a complete guide to DOM error handling strategies.

---

## How competing libraries handle diagnostics

The table below shows what you get from each library when a struct field
receives a wrong JSON type.

| Library | Lenient mode | Strict / error mode | Byte offset? |
|:---|:---|:---|:---|
| **qbuem-json** | Silent skip, keep default | `fuse_strict<T>()` or `QBUEM_NEXUS_STRICT` — throws `std::runtime_error` with type labels and byte offset | ✅ always |
| **nlohmann/json** | Throws `nlohmann::json::type_error` on `.get<T>()` — no "lenient" mode | Same exception, no byte offset | ❌ |
| **glaze** | Silent skip by default | `glaze::error_ctx` return value; no throw | ❌ (error code only) |
| **simdjson** | `simdjson::error_code` enum — caller must check every call | Same; throws only with `.value()` | ❌ |
| **RapidJSON** | Silent; writer/handler must validate manually | No built-in field type checking | ❌ |

Key differences:
- qbuem-json is the only library in this list that provides a **byte offset** pointing to the exact token that failed.
- The lenient default means **zero overhead** on the hot path — you pay nothing in production for the diagnostic capability.
- The thread-local strict flag means you can enable strict mode **per-call** without recompiling, which is useful for request-scoped validation in server code.

---

## Quick reference

| Scenario | Behaviour |
|:---|:---|
| Root JSON value is not an object | Always throws (both modes) |
| Field value has wrong type — `fuse<T>()` | Silent skip; field keeps default value |
| Field value has wrong type — `fuse_strict<T>()` | Throws `std::runtime_error` with type labels and byte offset |
| `std::optional<T>` field receives `null` | Sets to `std::nullopt`; no error (both modes) |
| `std::optional<T>` field receives wrong non-null type — strict | Throws with type labels and byte offset |
| Integer overflow / bad digits — `fuse<T>()` | Silent; field is `0` or partial |
| Integer overflow / bad digits — `fuse_strict<T>()` | Throws `"Nexus: integer parse failed …"` |
| Unknown JSON field | Always skipped (both modes) |
| `Value::as<T>()` DOM type mismatch | Always throws |
| `Value::try_as<T>()` DOM type mismatch | Returns `std::nullopt` |
