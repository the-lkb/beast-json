# Nexus Fusion: Zero-Tape Mapping

`beast::fuse<T>(text)` parses JSON directly into your C++ struct — no tape, no DOM, no intermediate state. When the parser encounters `"id"`, it hashes the key, looks up the corresponding struct member offset, and writes the value there. The tape is never built.

This page explains how that works, when it beats the DOM engine, and what it costs.

---

## What "0 KB allocation" actually means

The benchmarks report `0 KB` for struct parsing. That needs unpacking.

When we say 0 KB, we mean **structural allocation** — memory the parser itself uses to do its work. For the DOM engine, this is the tape. For Nexus, there is no tape, so it's literally zero.

The user's data — the `std::string` fields, `std::vector` elements, nested objects — still allocate when they need to. A struct with all `int` and `double` fields costs nothing beyond the struct itself. A struct with a `std::vector<std::string>` costs whatever the vector needs. That part is unavoidable and correct.

```cpp
struct Tick {         // fixed-size types only
    uint64_t seq;
    double   bid, ask;
    int      side;
};
// beast::fuse<Tick>(json) — zero heap allocation, period.

struct Order {        // contains dynamic types
    uint64_t    id;
    std::string symbol;   // ← one allocation for the string
};
// beast::fuse<Order>(json) — one allocation: the symbol string.
// The parser itself still uses zero.
```

---

## DOM vs Nexus: when to use which

The two engines are not competing — they handle different shapes of work.

**Use the DOM engine (`beast::parse`) when:**
- The JSON schema isn't known at compile time
- You need to inspect arbitrary keys or traverse unknown structure
- The document is large and you'll only read a small fraction of it
- You need mutations (`.set()`, merge patch)

**Use Nexus (`beast::fuse<T>` / `beast::read<T>`) when:**
- You have a fixed C++ struct and you want it filled as fast as possible
- You're in a hot loop processing thousands of identical messages per second
- You want zero tape allocation by construction, not as an optimization

A concrete HFT example: market data feeds deliver millions of JSON messages per day with the same shape. `beast::read<MarketTick>` on a warmed-up buffer processes each one without touching the allocator.

---

## How field dispatch works

The naive approach to mapping JSON keys to struct fields is string comparison — `if (key == "id") fill_id(val)` repeated for every field. This is O(N×F) where N is the number of keys and F is the number of fields. For a 20-field struct, that's 400 comparisons per object.

Nexus uses **compile-time FNV-1a hashing** to reduce this to O(1).

At compile time, `BEAST_JSON_FIELDS(User, id, name, active)` generates a hash for each field name:

```cpp
constexpr uint64_t hash_id     = fnv1a_hash_ce("id");     // 0xa9f37…
constexpr uint64_t hash_name   = fnv1a_hash_ce("name");   // 0xb4c28…
constexpr uint64_t hash_active = fnv1a_hash_ce("active"); // 0xf91a3…
```

At parse time, when the scanner sees the key `"name"`:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">Parsing <code>{"id": 42, "name": "Alice"}</code></div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step">
            <div class="bd-step__num">1</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Scan key</div>
              <div class="bd-step__desc">NexusScanner reads <code>"name"</code> — 4 bytes, no escape chars, SWAR fast-path.</div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">2</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Hash at runtime</div>
              <div class="bd-step__desc"><code>hash("name")</code> — same FNV-1a function, same result as the compile-time constant.</div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">3</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Switch on hash</div>
              <div class="bd-step__desc">Generated <code>switch(h)</code>: one case per field. Branch predictor wins on repeated schema.</div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">4</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Write directly</div>
              <div class="bd-step__desc"><code>std::from_chars</code> or <code>from_json_direct</code> writes value into <code>&obj.name</code>. No intermediate copy.</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

For keys ≤ 8 bytes, `fast_key_hash` loads the key as a single 64-bit integer (one `memcpy`), XOR-folds it, and returns. No loop, no byte-by-byte iteration.

---

## Unknown fields and malformed input

`beast::fuse<T>` doesn't fail on unknown fields — it skips them. A JSON payload with extra keys `{"id": 1, "debug_trace": {...}, "name": "Alice"}` will populate `id` and `name` and silently skip `debug_trace`. No allocation occurs during the skip.

Malformed JSON throws `std::runtime_error` with the stream position. The parser is single-pass, so detection is immediate.

Deep nesting doesn't cause stack overflow. The parser iterates rather than recurses, so `{"a": {"b": {"c": ...}}}` at any depth is handled with fixed stack space.

---

## BEAST_JSON_FIELDS macro

This macro is the entry point. It generates both the Nexus dispatch table and the DOM compatibility layer:

```cpp
struct User {
    uint64_t    id;
    std::string name;
    bool        active;
};
BEAST_JSON_FIELDS(User, id, name, active)

// Now both engines work:
auto doc = beast::parse(doc, json_text);
User u1  = doc.root().as<User>();       // DOM path: tape → struct

User u2;
beast::fuse(u2, json_text);             // Nexus path: stream → struct, no tape
```

The macro supports up to 32 fields. Beyond that, use the manual ADL hook API
(`from_beast_json` / `append_beast_json` free functions) — same performance, no limit.
