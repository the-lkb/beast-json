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

## Error messages produced by Nexus

### 1. Root-level type mismatch — always thrown

If the top-level JSON value passed to `fuse<T>()` is not a JSON object, Nexus
always throws regardless of strict mode:

```
Nexus: expected object, got array at byte offset 0
Nexus: expected object, got string at byte offset 0
Nexus: expected object, got end-of-input at byte offset 0
```

The byte offset is relative to the start of the JSON string.  This error is
unconditional because a C++ struct has no valid representation for anything
other than a JSON object.

### 2. Field-level type mismatches — strict mode only

In **strict mode** (see below), mismatches at the field level also throw:

```
Nexus: expected boolean, got number
Nexus: expected number, got string
Nexus: expected string, got object
Nexus: expected object, got array
Nexus: expected array, got null
Nexus: integer parse failed (overflow or unexpected digits)
```

When a `begin` pointer is available (always the case with `fuse_strict<T>()`),
byte offsets are included:

```
Nexus: expected number, got string at byte offset 42
```

The detected JSON types used in messages are:

| Detected token | Label in error |
|---|---|
| `{` | `object` |
| `[` | `array` |
| `"…"` | `string` |
| `true` / `false` | `boolean` |
| `null` | `null` |
| `-`, `0`–`9` | `number` |
| other / past end | `unknown token` / `end-of-input` |

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
call strict.  This is useful for debug/test builds:

```cmake
# CMakeLists.txt — debug build only
target_compile_definitions(my_tests PRIVATE QBUEM_NEXUS_STRICT)
```

```cpp
// Or in a single translation unit:
#define QBUEM_NEXUS_STRICT
#include <qbuem_json/qbuem_json.hpp>
```

When the macro is defined, `fuse_strict<T>()` and `fuse<T>()` behave
identically.

---

## Interpreting a type mismatch

### Example

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

Byte offset 16 points to the `"500"` value.  Counting from the start of the
JSON:

```
{"timeout_ms": "500", "retry": true}
0123456789012345678
                ^--- offset 16 = the opening '"' of "500"
```

### Nested structs

For nested structs mapped via `QBUEM_JSON_FIELDS`, the `NexusScanner::fill()`
call for the inner struct also checks that its slice of the JSON is an object.
A wrong type at any nesting level produces a clear message:

```
Nexus: expected object, got array at byte offset 23
```

---

## Mapping unknown fields

Unknown JSON fields are always silently skipped (both in lenient and strict
mode).  This is by design: Nexus is additive — extra fields in the JSON do not
cause errors.  If you need to detect unknown fields, parse with the DOM API
(`qbuem::parse`) and inspect the object keys manually.

---

## DOM type mismatch errors (Value::as<T>)

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

## Quick reference

| Scenario | Behaviour |
|---|---|
| Root JSON value is not an object | Always throws (both modes) |
| Field value has wrong type — `fuse<T>()` | Silent skip; field keeps default value |
| Field value has wrong type — `fuse_strict<T>()` | Throws `std::runtime_error` with type labels and offset |
| Integer overflow / bad digits — `fuse<T>()` | Silent; field is 0 or partial |
| Integer overflow / bad digits — `fuse_strict<T>()` | Throws `"Nexus: integer parse failed …"` |
| Unknown JSON field | Always skipped (both modes) |
| `Value::as<T>()` DOM type mismatch | Always throws |
| `Value::try_as<T>()` DOM type mismatch | Returns `std::nullopt` |
