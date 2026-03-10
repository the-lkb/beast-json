# Low-Latency Patterns

Beast JSON is engineered for workloads where parsing overhead must not stall execution. These patterns apply to any domain that processes JSON at high throughput or under strict timing constraints — trading systems, game engines, web API gateways, embedded devices, and data pipelines.

The core principle is always the same: **allocate once, reuse forever**.

---

## The Fundamental Pattern: Document Reuse

The single most impactful change for any hot loop is to stop calling `malloc` on every parse. Beast JSON's `Document` is designed for this — re-calling `beast::parse()` on the same document resets the tape cursor without freeing memory:

```cpp
#include <beast_json/beast_json.hpp>

beast::Document doc;           // allocated once at startup
// beast::Document doc; doc.reserve(64 * 1024);  // optional: pre-warm to avoid first-parse realloc

while (running) {
    auto msg = source.receive();                   // socket, queue, ring buffer, file line, …
    beast::Value root = beast::parse(doc, msg);    // zero malloc after first call
    if (!root.is_valid()) continue;

    dispatch(root["type"].as<std::string_view>(), root);
}
```

After the first parse warms the tape, every subsequent call in steady state makes **zero heap allocations**.

---

## Patterns by Domain

### Trading & Event-Driven Systems

Market data feeds and order routing systems deliver hundreds of thousands of JSON messages per second. A single stray allocation is a measurable latency spike.

```cpp
// Pinned to a dedicated CPU core — runs in a tight loop
beast::Document doc;
doc.reserve(2 * 1024); // typical market message < 1 KB

void on_market_message(std::string_view raw) {
    beast::Value root = beast::parse(doc, raw);
    if (!root.is_valid()) return;

    auto sym   = root["sym"].as<std::string_view>();
    auto price = root["px"].as<double>();
    auto qty   = root["qty"].as<int64_t>();
    auto side  = root["side"] | std::string_view{"B"};  // default if absent

    order_book.update(sym, price, qty, side);
}
```

**Why zero-alloc matters here:** a single `malloc` under contention can stall for 10–50 µs. With document reuse, the allocator is never touched after startup.

---

### Game Engines

Level data, save files, network state deltas, and config all use JSON. In a 60 Hz fixed-timestep loop, frame budget for I/O is measured in milliseconds.

```cpp
// One persistent document per purpose — pre-sized at load time
beast::Document level_doc;
beast::Document net_doc;

void load_level(std::string_view json) {
    beast::Value root = beast::parse(level_doc, json);

    for (auto [id_v, entity] : root["entities"].items()) {
        spawn_entity({
            .id       = entity["id"].as<uint64_t>(),
            .type     = entity["type"].as<std::string_view>(),
            .x        = entity["x"] | 0.0,
            .y        = entity["y"] | 0.0,
            .rotation = entity["rot"] | 0.0,
        });
    }
}

// Called every network tick — re-uses net_doc's tape
void on_net_delta(std::string_view delta) {
    beast::Value root = beast::parse(net_doc, delta);
    if (!root.is_valid()) return;

    for (auto state : root["states"].elements()) {
        uint64_t entity_id = state["eid"].as<uint64_t>();
        apply_delta(entity_id, state);
    }
}
```

**Key:** `level_doc` and `net_doc` are sized independently so a large level parse doesn't waste memory during normal net ticks.

---

### High-Throughput Web APIs

An API gateway at 50 k req/s cannot allocate a new DOM per request. Use a `thread_local` document — one per worker thread, no synchronization required:

```cpp
thread_local beast::Document tl_doc; // one per OS thread, zero contention

void handle_request(std::string_view body, HttpResponse& resp) {
    beast::Value root = beast::parse(tl_doc, body);
    if (!root.is_valid()) {
        resp.status(400).body("Bad JSON");
        return;
    }

    // Fallback defaults via | operator — never throws
    auto action  = root["action"]  | std::string_view{"unknown"};
    auto version = root["version"] | 1;
    auto dry_run = root["dry_run"] | false;

    dispatch(action, version, dry_run, root);
}
```

`thread_local` initialization happens once per thread on first use. After that, every request reuses the same pre-warmed tape.

---

### Embedded & IoT

On microcontrollers or systems without a reliable heap, use `std::pmr` to parse directly into a stack-allocated buffer — **no heap involved at all**:

```cpp
#include <memory_resource>

void handle_sensor_packet(std::string_view json) {
    // 16 KB lives entirely on the stack
    alignas(std::max_align_t) std::byte buf[16 * 1024];
    std::pmr::monotonic_buffer_resource pool(buf, sizeof(buf));

    auto doc  = beast::json::parse(json, &pool);
    auto root = doc.root();

    float  temp   = root["temp"].as<double>();
    int    sensor = root["id"].as<int64_t>();
    bool   alarm  = root["alarm"] | false;

    publish_reading(sensor, temp, alarm);
} // buf goes out of scope — nothing to free, no allocator lock
```

Suitable for hard real-time constraints where heap latency is non-deterministic. Combine with `doc.reserve()` if the stack buffer must be sized precisely.

---

### Telemetry & Log Processing Pipelines

Log ingestion and metrics processing parse millions of JSON lines per second. Batch reuse over an entire processing window:

```cpp
beast::Document doc;
doc.reserve(8 * 1024); // typical log line

struct Stats { uint64_t errors = 0, warnings = 0, total = 0; };

Stats process_log_chunk(std::span<std::string_view> lines) {
    Stats s;
    for (auto line : lines) {
        ++s.total;
        beast::Value root = beast::parse(doc, line);
        if (!root.is_valid()) { ++s.errors; continue; }

        auto level = root["level"] | std::string_view{""};
        if (level == "ERROR") {
            ++s.errors;
            emit_alert(root["msg"] | std::string_view{""},
                       root["ts"]  | int64_t{0});
        } else if (level == "WARN") {
            ++s.warnings;
        }
    }
    return s;
}
```

One `Document`, one tape allocation for the entire chunk — regardless of how many lines it contains.

---

## Technique Reference

| Technique | Allocation cost | Impact | Best for |
|:---|:---|:---|:---|
| `beast::parse_reuse(doc, …)` | **Zero** after warmup | ★★★★★ | Any hot loop (explicit reuse intent) |
| `beast::parse(doc, …)` reuse | **Zero** after warmup | ★★★★★ | Any hot loop (equivalent to above) |
| `thread_local Document` | **Zero** per request | ★★★★★ | Web servers, workers |
| `std::pmr` stack pool | **Zero** (no heap) | ★★★★★ | Embedded, hard RT |
| `doc.reserve(N)` at startup | One-time only | ★★★★☆ | All of the above |
| `| default` fallback access | None | ★★★☆☆ | Defensive field reads |
| Lazy `.as<T>()` extraction | Per field, on demand | ★★★☆☆ | Sparse access patterns |

---

## Branch Prediction & Cold Paths

Beast JSON uses `BEAST_LIKELY` / `BEAST_UNLIKELY` internally to keep the parsing hot path free of unpredictable branches. Error handling, escape processing, and UTF-8 validation are placed in cold sections — so the instruction cache sees only the fast path across sequential cache-warm iterations.

Your code benefits from this automatically; no annotation is required on the call site.

---

> [!TIP]
> For even tighter control over where Beast JSON allocates, see [Custom Allocators](/guide/allocators). Combining `std::pmr::monotonic_buffer_resource` with document reuse is the highest-performance configuration for throughput-critical workloads.
