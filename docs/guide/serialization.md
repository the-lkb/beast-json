# Serialization

qbuem-json's serializer is built for **extreme throughput**. It uses a Stream-Push model that writes directly to an output buffer — no intermediate allocations, no `sprintf`, no `std::to_string`.

---

## 🗺️ `dump()` vs `write()` — Which One Should You Use?

This is the most common source of confusion. Here is the short answer:

| You have… | Use… |
| :--- | :--- |
| A **parsed** `qbuem::Value` and want to re-serialize it (possibly after mutations) | **`value.dump()`** |
| A **C++ object** (struct, STL container, scalar) and want to convert it to JSON | **`qbuem::write(obj)`** |

### `value.dump()` — Back to JSON from a Parsed Document

Call `.dump()` on any `qbuem::Value` you got from `qbuem::parse()`. It serializes the node and everything below it, including any mutations you applied.

```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({"user": {"name": "Alice", "age": 25}})");

root["user"]["age"] = 26;           // mutate in-place
std::string json = root.dump();     // re-serialize the whole document
// → {"user":{"name":"Alice","age":26}}

std::string subtree = root["user"].dump();  // serialize only the subtree
// → {"name":"Alice","age":26}
```

**When to use `dump()`:**
- Round-tripping: parse → inspect/mutate → serialize back.
- Extracting or forwarding a JSON subtree.
- Any time you are working with data that came from `qbuem::parse()`.

### `qbuem::write()` — C++ Object to JSON

Use `qbuem::write()` when you are starting from a C++ object that was never parsed from JSON.

```cpp
struct Order { int id; double price; std::string symbol; };
QBUEM_JSON_FIELDS(Order, id, price, symbol)

Order o{42, 99.5, "AAPL"};
std::string json = qbuem::write(o);         // compact
std::string pretty = qbuem::write(o, 2);    // 2-space indented

> [!TIP]
> `qbuem::write` is universal. It works perfectly for structs whether you parse them via the **DOM Engine** (`qbuem::read`) or the **Nexus Engine** (`qbuem::fuse`).
```

**When to use `qbuem::write()`:**
- Serializing application structs / STL containers to send over a network or write to disk.
- Any time you are building JSON from scratch in C++, not from a previously parsed document.

### Want to Pretty-Print a Parsed Document?

Use `value.dump(indent)` — it takes an indent size just like `qbuem::write()`:

```cpp
std::string pretty = root.dump(2);   // 2-space pretty-print of a parsed Value
```

### Want to Re-Serialize to a Reusable Buffer?

```cpp
std::string buf;
buf.reserve(4096);

// From a parsed document (mutations reflected):
root.dump(buf);        // appends into buf — reuse across loop iterations
buf.clear();

// From a C++ object:
qbuem::write_to(buf, my_struct);
buf.clear();
```

---

## 🚀 Quick Start

```cpp
#include <qbuem_json/qbuem_json.hpp>

// Serialize any type with a single call
std::string json = qbuem::write(42);           // "42"
std::string json = qbuem::write(true);         // "true"
std::string json = qbuem::write("hello");      // "\"hello\""

std::vector<int> v = {1, 2, 3};
std::string json = qbuem::write(v);            // "[1,2,3]"

std::map<std::string, int> m = {{"a", 1}};
std::string json = qbuem::write(m);            // "{\"a\":1}"
```

---

## 📤 Serialization APIs

### `qbuem::write(value)` — The Unified Serializer

Works with **any supported type** out of the box.

```cpp
// --- Scalars ---
qbuem::write(42);           // "42"
qbuem::write(3.14);         // "3.14"
qbuem::write(true);         // "true"
qbuem::write(nullptr);      // "null"
qbuem::write("hello");      // "\"hello\""

// --- STL Containers ---
qbuem::write(std::vector<int>{1, 2, 3});                   // "[1,2,3]"
qbuem::write(std::array<double, 2>{1.1, 2.2});             // "[1.1,2.2]"
qbuem::write(std::map<std::string, int>{{"a",1},{"b",2}}); // "{\"a\":1,\"b\":2}"
qbuem::write(std::set<int>{3, 1, 2});                      // "[1,2,3]"

// --- Optional / Nullable ---
qbuem::write(std::optional<int>{42});     // "42"
qbuem::write(std::optional<int>{});      // "null"

// --- Tuples & Pairs ---
qbuem::write(std::tuple{1, "hello", true}); // "[1,\"hello\",true]"
qbuem::write(std::pair{"key", 99});         // "[\"key\",99]"
```

### `qbuem::write(value, indent)` — Pretty Print

```cpp
struct Config { std::string host; int port; };
QBUEM_JSON_FIELDS(Config, host, port)

Config cfg{"localhost", 8080};

// Compact
std::string compact = qbuem::write(cfg);
// {"host":"localhost","port":8080}

// Pretty with 2-space indent
std::string pretty = qbuem::write(cfg, 2);
// {
//   "host": "localhost",
//   "port": 8080
// }

// Pretty with 4-space indent
std::string pretty4 = qbuem::write(cfg, 4);
```

### `qbuem::write_to(buffer, value)` — Zero-Allocation Buffer Reuse

The highest-performance option. Appends to an existing `std::string` buffer to avoid repeated allocations in hot loops.

```cpp
std::string buffer;
buffer.reserve(4096); // warm up once

for (auto& event : event_stream) {
    buffer.clear();
    qbuem::write_to(buffer, event); // no malloc after buffer is warmed up!
    send_to_kafka(buffer);
}
```

### `Value::dump()` — Serialize a Parsed Value

When you have a `qbuem::Value` from a parsed document, you can serialize any subtree.

```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({"user": {"name": "Bob", "age": 25}, "active": true})");

// Serialize entire document
std::string full = root.dump();
// {"user":{"name":"Bob","age":25},"active":true}

// Serialize only the user subtree
std::string user = root["user"].dump();
// {"name":"Bob","age":25}

// Serialize a scalar
std::string name = root["user"]["name"].dump();
// "Bob"
```

### `Value::dump(indent)` — Pretty Print a Parsed Value

```cpp
std::string pretty = root.dump(2);
// {
//   "user": {
//     "name": "Bob",
//     "age": 25
//   },
//   "active": true
// }
```

### `Value::dump(buffer)` — Serialize into Existing Buffer

Perfect for repeated serialization of the same (potentially mutated) document.

```cpp
std::string buf;
buf.reserve(1024);

// Subsequent calls reuse the buffer's capacity
root["active"] = false;
root.dump(buf);
send(buf);
```

---

## 📦 Large Structs (> 16 Fields)

`QBUEM_JSON_FIELDS` is a variadic macro that supports up to **16 fields** per struct. If your struct has more, you have two options:

### Option A: Manual ADL Hooks (flat JSON layout preserved)

Define `from_qbuem_json`, `to_qbuem_json`, and `append_qbuem_json` free functions yourself in the same namespace. These are exactly what the macro generates internally — you are just writing them by hand.

```cpp
struct BigEvent {
    // 17 fields — one too many for QBUEM_JSON_FIELDS
    int64_t  seq;
    int64_t  ts;
    double   price;
    double   qty;
    double   bid;
    double   ask;
    double   bid_qty;
    double   ask_qty;
    int      side;
    int      type;
    bool     is_snapshot;
    bool     is_last;
    std::string symbol;
    std::string venue;
    std::string feed;
    std::string session;
    std::string trader_id;
};

// In the same namespace (or global if the struct is global):
inline void from_qbuem_json(const qbuem::json::Value& v, BigEvent& o) {
    qbuem::json::detail::from_json_field(v, "seq",       o.seq);
    qbuem::json::detail::from_json_field(v, "ts",        o.ts);
    qbuem::json::detail::from_json_field(v, "price",     o.price);
    // ... repeat for all 17 fields
    qbuem::json::detail::from_json_field(v, "trader_id", o.trader_id);
}

inline void to_qbuem_json(qbuem::json::Value& v, const BigEvent& o) {
    qbuem::json::detail::to_json_field(v, "seq",       o.seq);
    qbuem::json::detail::to_json_field(v, "ts",        o.ts);
    // ... repeat for all 17 fields
    qbuem::json::detail::to_json_field(v, "trader_id", o.trader_id);
}
```

See the [API Reference — Large Structs](/api/#large-structs-16-fields) for the complete `append_qbuem_json` signature used by `qbuem::write()` and `qbuem::write_to()`.

### Option B: Split into Sub-Structs (recommended if JSON shape is flexible)

Decompose the large struct into smaller nested structs, each with ≤ 16 fields, and use `QBUEM_JSON_FIELDS` on all of them:

```cpp
struct EventHeader {
    int64_t seq; int64_t ts; std::string symbol;
    std::string venue; std::string feed; std::string session;
};
struct EventPrices {
    double price; double qty; double bid; double ask;
    double bid_qty; double ask_qty; int side; int type;
};
struct EventFlags {
    bool is_snapshot; bool is_last; std::string trader_id;
};
struct BigEvent {
    EventHeader header;
    EventPrices prices;
    EventFlags  flags;
};

QBUEM_JSON_FIELDS(EventHeader, seq, ts, symbol, venue, feed, session)
QBUEM_JSON_FIELDS(EventPrices, price, qty, bid, ask, bid_qty, ask_qty, side, type)
QBUEM_JSON_FIELDS(EventFlags,  is_snapshot, is_last, trader_id)
QBUEM_JSON_FIELDS(BigEvent,    header, prices, flags)
```

> **Trade-off:** The JSON layout becomes nested (`"header": {...}, "prices": {...}`). If you need a flat JSON object with all fields at the top level, use Option A instead.

---

## ⚡ Performance Tips

### Tip 1: Reserve Your Buffer

```cpp
// Pre-estimate the output size to avoid reallocations
std::string buf;
buf.reserve(estimated_json_size);
qbuem::write_to(buf, my_data);
```

### Tip 2: Reuse Buffers in Loops

```cpp
CompetitorData competitor;
std::string output;
output.reserve(512);

for (const auto& match : live_matches) {
    fill(competitor, match);
    output.clear();
    qbuem::write_to(output, competitor); // zero-alloc after warmup
    publish(output);
}
```

### Tip 3: Use `string_view` for Read-Only Strings

When adding strings to your struct, use `std::string_view` where the source buffer is guaranteed to outlive the serialization call. This avoids a heap copy.

```cpp
struct LogEntry {
    std::string_view level;   // pointer to a static string — zero copy
    std::string message;       // owned string
    int64_t timestamp;
};
QBUEM_JSON_FIELDS(LogEntry, level, message, timestamp)
```

---

## 🔢 Number Serialization

qbuem-json uses **two purpose-built algorithms** instead of `std::to_chars` or `sprintf`:

### Integers — yy-itoa (Y. Yuan / yyjson, MIT)

Integer-to-decimal conversion via a 2-digit ASCII lookup table and multiply-shift
arithmetic — **zero division instructions** in the hot path. Dispatches to separate
32-bit and 64-bit code paths for maximum throughput.

```cpp
qbuem::write(9731);   // "9731"  — 3 multiplies, 2 table lookups, no div
qbuem::write(-42LL);  // "-42"
```

### Floats — Schubfach dtoa (R. Giulietti 2020, via yyjson, MIT)

The Schubfach algorithm produces the **shortest decimal string that round-trips
exactly** through any IEEE 754-conformant parser. Implemented with a 128-bit pow10
table (1,336 precomputed constants) — no floating-point arithmetic, no fallback path.

- **Bit-accurate round-tripping**: Parsing the output gives back the exact same `double`.
- **Shortest representation**: Trailing zeros trimmed automatically.
- **Special values**: `NaN` and `±Inf` serialize as `null` (RFC 8259 compliant).

```cpp
double pi = 3.141592653589793;

qbuem::write(pi);  // "3.141592653589793"
// Not "3.14159265358979300" — trailing zeros stripped

qbuem::write(3.0); // "3"   — integer-looking doubles stay compact
qbuem::write(std::numeric_limits<double>::infinity()); // "null"
```

> [!NOTE]
> **Russ Cox + Eisel-Lemire** are used for number **parsing** (input → `double`).
> **Schubfach + yy-itoa** are used for number **serialization** (number → string).
> See [Numeric Serialization Theory](/theory/numeric-serialization) for a deep-dive
> with diagrams, and [Acknowledgments](/guide/acknowledgments) for full attribution.
