# Serialization

Beast JSON's serializer is built for **extreme throughput**. It uses a Stream-Push model that writes directly to an output buffer — no intermediate allocations, no `sprintf`, no `std::to_string`.

---

## 🗺️ `dump()` vs `write()` — Which One Should You Use?

This is the most common source of confusion. Here is the short answer:

| You have… | Use… |
| :--- | :--- |
| A **parsed** `beast::Value` and want to re-serialize it (possibly after mutations) | **`value.dump()`** |
| A **C++ object** (struct, STL container, scalar) and want to convert it to JSON | **`beast::write(obj)`** |

### `value.dump()` — Back to JSON from a Parsed Document

Call `.dump()` on any `beast::Value` you got from `beast::parse()`. It serializes the node and everything below it, including any mutations you applied.

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"user": {"name": "Alice", "age": 25}})");

root["user"]["age"] = 26;           // mutate in-place
std::string json = root.dump();     // re-serialize the whole document
// → {"user":{"name":"Alice","age":26}}

std::string subtree = root["user"].dump();  // serialize only the subtree
// → {"name":"Alice","age":26}
```

**When to use `dump()`:**
- Round-tripping: parse → inspect/mutate → serialize back.
- Extracting or forwarding a JSON subtree.
- Any time you are working with data that came from `beast::parse()`.

### `beast::write()` — C++ Object to JSON

Use `beast::write()` when you are starting from a C++ object that was never parsed from JSON.

```cpp
struct Order { int id; double price; std::string symbol; };
BEAST_JSON_FIELDS(Order, id, price, symbol)

Order o{42, 99.5, "AAPL"};
std::string json = beast::write(o);         // compact
std::string pretty = beast::write(o, 2);    // 2-space indented
```

**When to use `beast::write()`:**
- Serializing application structs / STL containers to send over a network or write to disk.
- Any time you are building JSON from scratch in C++, not from a previously parsed document.

### Want to Pretty-Print a Parsed Document?

Use `value.dump(indent)` — it takes an indent size just like `beast::write()`:

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
beast::write_to(buf, my_struct);
buf.clear();
```

---

## 🚀 Quick Start

```cpp
#include <beast_json/beast_json.hpp>

// Serialize any type with a single call
std::string json = beast::write(42);           // "42"
std::string json = beast::write(true);         // "true"
std::string json = beast::write("hello");      // "\"hello\""

std::vector<int> v = {1, 2, 3};
std::string json = beast::write(v);            // "[1,2,3]"

std::map<std::string, int> m = {{"a", 1}};
std::string json = beast::write(m);            // "{\"a\":1}"
```

---

## 📤 Serialization APIs

### `beast::write(value)` — The Unified Serializer

Works with **any supported type** out of the box.

```cpp
// --- Scalars ---
beast::write(42);           // "42"
beast::write(3.14);         // "3.14"
beast::write(true);         // "true"
beast::write(nullptr);      // "null"
beast::write("hello");      // "\"hello\""

// --- STL Containers ---
beast::write(std::vector<int>{1, 2, 3});                   // "[1,2,3]"
beast::write(std::array<double, 2>{1.1, 2.2});             // "[1.1,2.2]"
beast::write(std::map<std::string, int>{{"a",1},{"b",2}}); // "{\"a\":1,\"b\":2}"
beast::write(std::set<int>{3, 1, 2});                      // "[1,2,3]"

// --- Optional / Nullable ---
beast::write(std::optional<int>{42});     // "42"
beast::write(std::optional<int>{});      // "null"

// --- Tuples & Pairs ---
beast::write(std::tuple{1, "hello", true}); // "[1,\"hello\",true]"
beast::write(std::pair{"key", 99});         // "[\"key\",99]"
```

### `beast::write(value, indent)` — Pretty Print

```cpp
struct Config { std::string host; int port; };
BEAST_JSON_FIELDS(Config, host, port)

Config cfg{"localhost", 8080};

// Compact
std::string compact = beast::write(cfg);
// {"host":"localhost","port":8080}

// Pretty with 2-space indent
std::string pretty = beast::write(cfg, 2);
// {
//   "host": "localhost",
//   "port": 8080
// }

// Pretty with 4-space indent
std::string pretty4 = beast::write(cfg, 4);
```

### `beast::write_to(buffer, value)` — Zero-Allocation Buffer Reuse

The highest-performance option. Appends to an existing `std::string` buffer to avoid repeated allocations in hot loops.

```cpp
std::string buffer;
buffer.reserve(4096); // warm up once

for (auto& event : event_stream) {
    buffer.clear();
    beast::write_to(buffer, event); // no malloc after buffer is warmed up!
    send_to_kafka(buffer);
}
```

### `Value::dump()` — Serialize a Parsed Value

When you have a `beast::Value` from a parsed document, you can serialize any subtree.

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"user": {"name": "Bob", "age": 25}, "active": true})");

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

`BEAST_JSON_FIELDS` is a variadic macro that supports up to **16 fields** per struct. If your struct has more, you have two options:

### Option A: Manual ADL Hooks (flat JSON layout preserved)

Define `from_beast_json`, `to_beast_json`, and `append_beast_json` free functions yourself in the same namespace. These are exactly what the macro generates internally — you are just writing them by hand.

```cpp
struct BigEvent {
    // 17 fields — one too many for BEAST_JSON_FIELDS
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
inline void from_beast_json(const beast::json::Value& v, BigEvent& o) {
    beast::json::detail::from_json_field(v, "seq",       o.seq);
    beast::json::detail::from_json_field(v, "ts",        o.ts);
    beast::json::detail::from_json_field(v, "price",     o.price);
    // ... repeat for all 17 fields
    beast::json::detail::from_json_field(v, "trader_id", o.trader_id);
}

inline void to_beast_json(beast::json::Value& v, const BigEvent& o) {
    beast::json::detail::to_json_field(v, "seq",       o.seq);
    beast::json::detail::to_json_field(v, "ts",        o.ts);
    // ... repeat for all 17 fields
    beast::json::detail::to_json_field(v, "trader_id", o.trader_id);
}
```

See the [API Reference — Large Structs](/api/#large-structs-16-fields) for the complete `append_beast_json` signature used by `beast::write()` and `beast::write_to()`.

### Option B: Split into Sub-Structs (recommended if JSON shape is flexible)

Decompose the large struct into smaller nested structs, each with ≤ 16 fields, and use `BEAST_JSON_FIELDS` on all of them:

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

BEAST_JSON_FIELDS(EventHeader, seq, ts, symbol, venue, feed, session)
BEAST_JSON_FIELDS(EventPrices, price, qty, bid, ask, bid_qty, ask_qty, side, type)
BEAST_JSON_FIELDS(EventFlags,  is_snapshot, is_last, trader_id)
BEAST_JSON_FIELDS(BigEvent,    header, prices, flags)
```

> **Trade-off:** The JSON layout becomes nested (`"header": {...}, "prices": {...}`). If you need a flat JSON object with all fields at the top level, use Option A instead.

---

## ⚡ Performance Tips

### Tip 1: Reserve Your Buffer

```cpp
// Pre-estimate the output size to avoid reallocations
std::string buf;
buf.reserve(estimated_json_size);
beast::write_to(buf, my_data);
```

### Tip 2: Reuse Buffers in Loops

```cpp
CompetitorData competitor;
std::string output;
output.reserve(512);

for (const auto& match : live_matches) {
    fill(competitor, match);
    output.clear();
    beast::write_to(output, competitor); // zero-alloc after warmup
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
BEAST_JSON_FIELDS(LogEntry, level, message, timestamp)
```

---

## 🔢 Float Precision

Beast JSON uses the **Russ Cox + Eisel-Lemire** hybrid algorithm for number serialization. It guarantees:

- **Bit-accurate round-tripping**: Parsing the serialized output gives you back the exact same `double`.
- **Shortest representation**: It always produces the shortest possible decimal that round-trips correctly.

```cpp
double pi = 3.141592653589793;

beast::write(pi);  // "3.141592653589793"
// Not "3.14159265358979300" or "3.1415926535897931"
```
