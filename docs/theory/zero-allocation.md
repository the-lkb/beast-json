# Zero-Allocation Principle

The single greatest source of latency jitter in C++ is the heap. Every `malloc` call can stall a thread for microseconds. Beast JSON eliminates all heap allocations from its hot path through three complementary techniques.

---

## Why Heap Allocation Hurts

A single `malloc` is not a simple operation. Under the hood, it involves:

```mermaid
flowchart TB
    CALL["malloc(n) called"]

    subgraph CHAIN["Hidden costs inside the allocator"]
        direction TB
        L["1. Acquire global allocator mutex<br/>(may block if another thread holds the lock)"]
        SZ["2. Size-class lookup<br/>(find a free block in the free list)"]
        SYS["3. Possible mmap / brk syscall<br/>(kernel mode switch: ~1,000 ns)"]
        CM["4. Cold cache miss on returned memory<br/>(TLB miss + page fault: ~100 ns)"]
        FR["5. Fragmentation accumulates over time<br/>(RSS grows even if logical usage stays flat)"]
        L --> SZ --> SYS --> CM --> FR
    end

    RET["Pointer returned to caller<br/>(after 50–5,000 ns depending on system state)"]
    CALL --> CHAIN --> RET
```

For a library parsing thousands of messages per second, these costs compound into **milliseconds of unbudgeted latency per second**. A single 5 μs stall can cascade into a missed deadline in HFT or game-server contexts.

---

## The Three-Layer Solution

Beast JSON eliminates heap allocations with three coordinated techniques:

```mermaid
flowchart TB
    subgraph L1["Layer 1 — Pre-allocated Tape"]
        T1["beast::Document owns one contiguous tape[]<br/>All TapeNodes written here<br/>Capacity grows once, reused forever"]
    end

    subgraph L2["Layer 2 — Zero-Copy String Views"]
        T2["beast::Value stores std::string_view<br/>Points into the caller's input buffer<br/>No memcpy, no heap string"]
    end

    subgraph L3["Layer 3 — Stream-Push Serialization"]
        T3["beast::write_to(buffer, value)<br/>Writes tokens directly into caller's buffer<br/>No intermediate tree, no temporary string"]
    end

    L1 --> L2 --> L3
```

---

## Layer 1: Tape Pre-Allocation and Reuse

`beast::Document` allocates its internal tape **once** on first use. Every subsequent `parse()` call on the same `Document` resets the write pointer to zero — reusing existing memory without any allocator involvement:

```mermaid
flowchart TB
    subgraph FIRST["First parse()"]
        direction TB
        F1["Document constructed<br/>tape = nullptr, capacity = 0"]
        F2["parse() called<br/>1× realloc to N slots"]
        F3["tape: capacity=N, used=k"]
        F1 --> F2 --> F3
    end

    subgraph SECOND["Second parse() — same Document"]
        direction TB
        S1["tape: capacity=N (unchanged)"]
        S2["write_head_ = 0 (reset only)<br/>zero malloc — buffer already large enough"]
        S3["tape: capacity=N, used=k2"]
        S1 --> S2 --> S3
    end

    F3 -->|"reuse same memory"| S1
```

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

```mermaid
flowchart TB
    G1["capacity = 0 (initial)"]
    G2["capacity = 256 (first doc: 200 nodes)"]
    G3["capacity = 512 (doc with 300 nodes)"]
    G4["capacity = 1024 (doc with 600 nodes)"]
    G5["capacity = 1024 (stable)<br/>all future docs → zero allocations"]

    G1 -->|"1× realloc"| G2
    G2 -->|"1× realloc"| G3
    G3 -->|"1× realloc"| G4
    G4 --- G5
```

In practice, documents in a single application tend to have stable schemas — after a few warmup parses, capacity stabilizes and no further allocations occur.

---

## Layer 2: Zero-Copy String Views

When Beast JSON encounters a string literal, it does **not** allocate a `std::string` or call `memcpy`. Instead, the `KEY` or `STRING` TapeNode stores a `std::string_view` pointing directly into the caller's input buffer:

```mermaid
flowchart TB
    subgraph BUF["Input Buffer — caller-owned (stack, recv buffer, mmap, etc.)"]
        RAW["{ #quot;name#quot;: #quot;Bob#quot; }"]
    end

    subgraph TAPE["Document Tape"]
        TN1["tape[1] KEY<br/>string_view { &amp;buf[2], len=4 } → 'name'"]
        TN2["tape[2] STRING<br/>string_view { &amp;buf[9], len=3 } → 'Bob'"]
    end

    TN1 -->|"zero-copy pointer"| BUF
    TN2 -->|"zero-copy pointer"| BUF
```

Accessing `root["name"]` returns a `string_view` pointing at `buf[2]` with `len=4`. **Zero bytes are allocated, zero bytes are copied.**

> **Lifetime rule**: `string_view` values are valid as long as both the `Document` and the input buffer are alive. Do not hold a `string_view` after either goes out of scope.

### Contrast with `nlohmann/json`

```mermaid
flowchart TB
    subgraph NLO["nlohmann::json — string storage"]
        direction TB
        N1["Input buffer: { name: Bob }"]
        N2["malloc(4) → 'name' (new std::string)"]
        N3["malloc(3) → 'Bob' (new std::string)"]
        N4["... × N for every string in document"]
        N1 --> N2 & N3 & N4
    end

    subgraph BEAST["beast::json — string storage"]
        direction TB
        B1["Input buffer: { name: Bob }"]
        B2["string_view { &amp;buf[2], 4 } — 8 bytes, no heap"]
        B3["string_view { &amp;buf[9], 3 } — 8 bytes, no heap"]
        B1 --> B2 & B3
    end
```

---

## Layer 3: Stream-Push Serialization

Traditional serializers construct an intermediate in-memory JSON tree, then walk it to produce the output string. Beast JSON uses a **stream-push model**: it walks your data structure once and writes tokens directly into the output buffer:

```mermaid
flowchart TB
    subgraph OLD["Traditional serializer — two-phase"]
        direction TB
        D1["User data (your structs)"]
        D2["Intermediate JSON tree<br/>(heap allocated — O(N) nodes)"]
        D3["Output string (second O(N) pass)"]
        D1 -->|"phase 1: build tree (N mallocs)"| D2
        D2 -->|"phase 2: stringify (1 malloc)"| D3
    end

    subgraph NEW["Beast JSON — stream-push"]
        direction TB
        E1["User data (your structs)"]
        E2["Output buffer (caller pre-allocated)"]
        E1 -->|"single pass: write directly (zero intermediate copies)"| E2
    end
```

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
