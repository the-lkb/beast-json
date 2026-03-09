# Zero-Allocation Principle

The single greatest source of latency jitter in C++ is the heap. Every `malloc` call can stall a thread for microseconds. Beast JSON eliminates all heap allocations from its hot path through three complementary techniques.

---

## Why Heap Allocation Hurts

A single `malloc` is not a simple operation. Under the hood, it involves:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--red">malloc(n) called</div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="width:100%;max-width:480px;">
      <div class="bd-group__title">Hidden costs inside the allocator</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step"><div class="bd-step__num">1</div><div class="bd-step__body"><div class="bd-step__title">Acquire global allocator mutex</div><div class="bd-step__desc">May block if another thread holds the lock</div></div></div>
          <div class="bd-step"><div class="bd-step__num">2</div><div class="bd-step__body"><div class="bd-step__title">Size-class lookup</div><div class="bd-step__desc">Find a free block in the free list</div></div></div>
          <div class="bd-step"><div class="bd-step__num">3</div><div class="bd-step__body"><div class="bd-step__title">Possible mmap / brk syscall</div><div class="bd-step__desc">Kernel mode switch: ~1,000 ns</div></div></div>
          <div class="bd-step"><div class="bd-step__num">4</div><div class="bd-step__body"><div class="bd-step__title">Cold cache miss on returned memory</div><div class="bd-step__desc">TLB miss + page fault: ~100 ns</div></div></div>
          <div class="bd-step"><div class="bd-step__num">5</div><div class="bd-step__body"><div class="bd-step__title">Fragmentation accumulates</div><div class="bd-step__desc">RSS grows even if logical usage stays flat</div></div></div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-box bd-box--orange">Pointer returned to caller<br><small>(after 50–5,000 ns depending on system state)</small></div>
  </div>
</div>

For a library parsing thousands of messages per second, these costs compound into **milliseconds of unbudgeted latency per second**. A single 5 μs stall can cascade into a missed deadline in HFT or game-server contexts.

---

## The Three-Layer Solution

Beast JSON eliminates heap allocations with three coordinated techniques:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-steps" style="max-width:540px;width:100%;">
      <div class="bd-step">
        <div class="bd-step__num" style="background:#0097a7;">1</div>
        <div class="bd-step__body">
          <div class="bd-step__title" style="color:#0097a7;">Layer 1 — Pre-allocated Tape</div>
          <div class="bd-step__desc">beast::Document owns one contiguous tape[]. All TapeNodes written here. Capacity grows once, reused forever.</div>
        </div>
      </div>
      <div class="bd-step">
        <div class="bd-step__num" style="background:#9c27b0;">2</div>
        <div class="bd-step__body">
          <div class="bd-step__title" style="color:#9c27b0;">Layer 2 — Zero-Copy String Views</div>
          <div class="bd-step__desc">beast::Value stores std::string_view pointing into the caller's input buffer. No memcpy, no heap string.</div>
        </div>
      </div>
      <div class="bd-step">
        <div class="bd-step__num" style="background:#4caf50;">3</div>
        <div class="bd-step__body">
          <div class="bd-step__title" style="color:#4caf50;">Layer 3 — Stream-Push Serialization</div>
          <div class="bd-step__desc">beast::write_to(buffer, value) writes tokens directly into the caller's buffer. No intermediate tree, no temporary string.</div>
        </div>
      </div>
    </div>
  </div>
</div>

---

## Layer 1: Tape Pre-Allocation and Reuse

`beast::Document` allocates its internal tape **once** on first use. Every subsequent `parse()` call on the same `Document` resets the write pointer to zero — reusing existing memory without any allocator involvement:

<div class="bd-diagram">
  <div class="bd-split" style="max-width:600px;margin:0 auto;">
    <div class="bd-group">
      <div class="bd-group__title">First parse()</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.78rem;">Document constructed<br><small>tape = nullptr, capacity = 0</small></div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--orange" style="font-size:0.78rem;">parse() called<br><small>1× realloc to N slots</small></div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--teal" style="font-size:0.78rem;">tape: capacity=N, used=k</div>
      </div>
    </div>
    <div class="bd-group">
      <div class="bd-group__title">Second parse() — same Document</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.78rem;">tape: capacity=N<br><small>(unchanged)</small></div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--green" style="font-size:0.78rem;">write_head_ = 0 (reset only)<br><small>zero malloc — buffer already large enough</small></div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--teal" style="font-size:0.78rem;">tape: capacity=N, used=k2</div>
      </div>
    </div>
  </div>
</div>

### What this looks like in a hot loop

```cpp
beast::Document doc;         // tape is empty — no allocation yet

while (true) {
    auto msg  = recv_message();
    auto root = beast::parse(doc, msg);  // zero malloc after first call
    handle(root);
    // tape is implicitly reused on the next loop iteration
}
```

After the first message warms up the tape, **every subsequent parse is allocation-free**.

### Tape capacity growth policy

The tape uses a doubling strategy. If an incoming document requires more nodes than the current capacity, `realloc` is called once to double the buffer:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-pipeline" style="flex-direction:column;align-items:stretch;max-width:400px;margin:0 auto;">
      <div class="bd-pipe-stage"><div class="bd-pipe-stage__label">Initial</div><div class="bd-pipe-stage__main">capacity = 0</div></div>
      <div class="bd-pipe-arrow" style="align-self:center;transform:rotate(90deg);">→</div>
      <div class="bd-pipe-stage bd-box--orange" style="border-color:#ff9800;background:rgba(255,152,0,0.06);"><div class="bd-pipe-stage__label">1× realloc</div><div class="bd-pipe-stage__main">capacity = 256</div><div class="bd-pipe-stage__note">first doc (200 nodes)</div></div>
      <div class="bd-pipe-arrow" style="align-self:center;transform:rotate(90deg);">→</div>
      <div class="bd-pipe-stage bd-box--orange" style="border-color:#ff9800;background:rgba(255,152,0,0.06);"><div class="bd-pipe-stage__label">1× realloc</div><div class="bd-pipe-stage__main">capacity = 512</div><div class="bd-pipe-stage__note">doc with 300 nodes</div></div>
      <div class="bd-pipe-arrow" style="align-self:center;transform:rotate(90deg);">→</div>
      <div class="bd-pipe-stage bd-box--orange" style="border-color:#ff9800;background:rgba(255,152,0,0.06);"><div class="bd-pipe-stage__label">1× realloc</div><div class="bd-pipe-stage__main">capacity = 1024</div><div class="bd-pipe-stage__note">doc with 600 nodes</div></div>
      <div class="bd-pipe-arrow" style="align-self:center;transform:rotate(90deg);">→</div>
      <div class="bd-pipe-stage bd-box--green" style="border-color:#4caf50;background:rgba(76,175,80,0.08);"><div class="bd-pipe-stage__label">Stable</div><div class="bd-pipe-stage__main">capacity = 1024</div><div class="bd-pipe-stage__note">all future docs → zero allocations</div></div>
    </div>
  </div>
</div>

In practice, documents in a single application tend to have stable schemas — after a few warmup parses, capacity stabilizes and no further allocations occur.

---

## Layer 2: Zero-Copy String Views

When Beast JSON encounters a string literal, it does **not** allocate a `std::string` or call `memcpy`. Instead, the `KEY` or `STRING` TapeNode stores a `std::string_view` pointing directly into the caller's input buffer:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:480px;">
      <div class="bd-group__title">Input Buffer — caller-owned (stack / recv buf / mmap)</div>
      <div class="bd-group__body">
        <div class="bd-box bd-box--blue">{ "name": "Bob" }</div>
      </div>
    </div>
    <div class="bd-row" style="gap:3rem;">
      <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">zero-copy pointer</div></div>
      <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">zero-copy pointer</div></div>
    </div>
    <div class="bd-group" style="width:100%;max-width:480px;">
      <div class="bd-group__title">Document Tape</div>
      <div class="bd-group__body">
        <div class="bd-tape-strip" style="justify-content:center;">
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[1] KEY</span><span class="bd-tape-cell__tag">string_view</span><span class="bd-tape-cell__val">&amp;buf[2], len=4 → "name"</span></div>
          <div class="bd-tape-cell bd-tape-cell--str"><span class="bd-tape-cell__idx">tape[2] STRING</span><span class="bd-tape-cell__tag">string_view</span><span class="bd-tape-cell__val">&amp;buf[9], len=3 → "Bob"</span></div>
        </div>
      </div>
    </div>
  </div>
</div>

Accessing `root["name"]` returns a `string_view` pointing at `buf[2]` with `len=4`. **Zero bytes are allocated, zero bytes are copied.**

> **Lifetime rule**: `string_view` values are valid as long as both the `Document` and the input buffer are alive. Do not hold a `string_view` after either goes out of scope.

### Contrast with `nlohmann/json`

<div class="bd-diagram">
  <div class="bd-split" style="max-width:600px;margin:0 auto;gap:1rem;">
    <div class="bd-group">
      <div class="bd-group__title" style="color:#f44336;border-bottom-color:#f44336;">nlohmann::json</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.78rem;">Input: { name: Bob }</div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--red" style="font-size:0.75rem;">malloc(4) → "name"<br><small>(new std::string)</small></div>
        <div class="bd-box bd-box--red" style="font-size:0.75rem;">malloc(3) → "Bob"<br><small>(new std::string)</small></div>
        <div class="bd-box bd-box--red" style="font-size:0.75rem;">… × N for every string</div>
      </div>
    </div>
    <div class="bd-group">
      <div class="bd-group__title" style="color:#0097a7;border-bottom-color:#0097a7;">beast::json</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.78rem;">Input: { name: Bob }</div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--green" style="font-size:0.75rem;">string_view { &amp;buf[2], 4 }<br><small>8 bytes — no heap</small></div>
        <div class="bd-box bd-box--green" style="font-size:0.75rem;">string_view { &amp;buf[9], 3 }<br><small>8 bytes — no heap</small></div>
      </div>
    </div>
  </div>
</div>

---

## Layer 3: Stream-Push Serialization

Traditional serializers construct an intermediate in-memory JSON tree, then walk it to produce the output string. Beast JSON uses a **stream-push model**: it walks your data structure once and writes tokens directly into the output buffer:

<div class="bd-diagram">
  <div class="bd-split" style="max-width:640px;margin:0 auto;gap:1rem;">
    <div class="bd-group">
      <div class="bd-group__title">Traditional — two-phase</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.78rem;">User data (your structs)</div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">phase 1: build tree (N mallocs)</div></div>
        <div class="bd-box bd-box--red" style="font-size:0.78rem;">Intermediate JSON tree<br><small>heap allocated — O(N) nodes</small></div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">phase 2: stringify (1 malloc)</div></div>
        <div class="bd-box" style="font-size:0.78rem;">Output string</div>
      </div>
    </div>
    <div class="bd-group">
      <div class="bd-group__title">Beast JSON — stream-push</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.78rem;">User data (your structs)</div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">single pass · zero intermediate · zero copies</div></div>
        <div class="bd-box bd-box--green" style="font-size:0.78rem;">Output buffer<br><small>(caller pre-allocated)</small></div>
      </div>
    </div>
  </div>
</div>

```cpp
std::string buf;
buf.reserve(8192);          // warm up once

for (auto& event : stream) {
    buf.clear();
    beast::write_to(buf, event);   // zero malloc — writes directly into buf
    send_to_kafka(buf);
}
```

After the first call warms the buffer, **every subsequent serialization is allocation-free**.

---

## Allocation Profile: Measured on twitter.json (631 KB)

| Operation | nlohmann/json | simdjson | Beast JSON |
|:---|---:|---:|---:|
| **Allocations per parse** | ~11,000 | 2 (workspace) | **0** (after warmup) |
| **Allocations per serialize** | ~5,000 | N/A (read-only) | **0** (with `write_to`) |
| **Peak RSS** | 27.4 MB | 11.0 MB | **3.4 MB** |
| **Heap fragmentation (1M calls)** | severe | moderate | **none** |
| **Allocator lock contention** | high | low | **zero** |

---

## Latency Percentile Impact

For real-time systems, **tail latency** matters more than average. Heap allocations cause unpredictable spikes:

| Percentile | nlohmann/json | Beast JSON |
|:---|---:|---:|
| **p50** | 6 μs | **0.3 μs** |
| **p99** | 47 μs ← malloc pressure | **0.4 μs** |
| **p99.9** | 312 μs ← OS page fault | **0.5 μs** |

For systems with a 1 μs parse budget (co-located HFT, kernel-bypass networking, FPGA gateway), the difference between `nlohmann` and Beast JSON is not "faster" — it is the difference between **viable** and **not viable**.
