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
        L["1. Acquire global allocator mutex\n   (may block if another thread holds the lock)"]
        SZ["2. Size-class lookup\n   (find a free block of the right size in the free list)"]
        SYS["3. Possible mmap / brk syscall\n   (if free list is empty — kernel mode switch: ~1,000 ns)"]
        CM["4. Cold cache miss on returned memory\n   (new pages not in L1/L2 — TLB miss + page fault: ~100 ns)"]
        FR["5. Fragmentation accumulates over time\n   (long-running process RSS grows even if logical usage stays flat)"]
        L --> SZ --> SYS --> CM --> FR
    end

    RET["Pointer returned to caller\n(after 50–5,000 ns depending on system state)"]
    CALL --> CHAIN --> RET
```

For a library parsing thousands of messages per second, these costs multiply into **milliseconds of unbudgeted latency per second**. In HFT or game-server contexts, a single 5 μs stall can cascade into a missed deadline.

---

## The Three-Layer Solution

Beast JSON eliminates heap allocations with three coordinated techniques:

```mermaid
flowchart TB
    subgraph L1["Layer 1 — Pre-allocated Tape"]
        direction LR
        T1["beast::Document owns one contiguous tape[]\nAll TapeNodes are written here\nCapacity grows once, reused forever"]
    end

    subgraph L2["Layer 2 — Zero-Copy String Views"]
        direction LR
        T2["beast::Value stores std::string_view\nPoints into the caller's input buffer\nNo memcpy, no heap string"]
    end

    subgraph L3["Layer 3 — Stream-Push Serialization"]
        direction LR
        T3["beast::write_to(buffer, value)\nWrites tokens directly into caller's buffer\nNo intermediate tree, no temporary string"]
    end

    L1 --> L2 --> L3
```

---

## Layer 1: Tape Pre-Allocation and Reuse

`beast::Document` allocates its internal tape **once** on first use. Every subsequent `parse()` call on the same `Document` resets the write pointer to zero — reusing the existing memory without any allocator involvement:

```mermaid
flowchart LR
    subgraph FIRST["First parse()"]
        direction TB
        F1["Document constructed\ntape_ capacity = 0\ntape_ data = nullptr"]
        F2["parse() called\nbuffer too small → realloc to N slots\n(1 malloc — happens once ever)"]
        F3["tape_ capacity = N\ntape_ used = k\n(k TapeNodes written)"]
        F1 --> F2 --> F3
    end

    subgraph SECOND["Second parse() — same Document"]
        direction TB
        S1["tape_ capacity = N (unchanged)\ntape_ used = k (old parse)"]
        S2["parse() called\nwrite_head_ = 0  (reset, not free)\nno malloc — buffer is already large enough"]
        S3["tape_ capacity = N\ntape_ used = k2\n(new parse, old memory)"]
        S1 --> S2 --> S3
    end

    F3 -->|"reuse"| S1
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

After the first message warms up the tape, **every subsequent parse is allocation-free** regardless of how many elements the document contains.

### Tape capacity growth policy

The tape uses a doubling strategy. If an incoming document requires more nodes than the current capacity, `realloc` is called once to double the buffer:

```mermaid
flowchart LR
    subgraph GROW["Capacity growth — amortized O(1)"]
        direction LR
        G1["capacity = 0\n(initial)"]
        G2["capacity = 256\n(first doc: 200 nodes)"]
        G3["capacity = 512\n(doc with 300 nodes)"]
        G4["capacity = 1024\n(doc with 600 nodes)"]
        G5["capacity = 1024\n(all future docs ≤ 1024 nodes\n→ zero allocations)"]
        G1 -->|"1× realloc"| G2 -->|"1× realloc"| G3 -->|"1× realloc"| G4 --- G5
    end
```

In practice, documents in a single application tend to have stable schemas — after a few warmup parses, capacity stabilizes and no further allocations occur.

---

## Layer 2: Zero-Copy String Views

When Beast JSON encounters a string literal, it does **not** allocate a `std::string` or call `memcpy`. Instead, the `KEY` or `STRING` TapeNode stores a `std::string_view` whose `.data()` pointer points directly into the caller's input buffer:

```mermaid
flowchart TB
    subgraph IBUF["Input Buffer — caller-owned (stack, recv buffer, mmap, etc.)"]
        direction LR
        B0["[0]\n'{'"]
        B1["[1]\n'\"'"]
        B2["[2]\n'n'"]
        B3["[3]\n'a'"]
        B4["[4]\n'm'"]
        B5["[5]\n'e'"]
        B6["[6]\n'\"'"]
        B7["[7]\n':'"]
        B8["[8]\n'\"'"]
        B9["[9]\n'B'"]
        B10["[10]\n'o'"]
        B11["[11]\n'b'"]
        B12["[12]\n'\"'"]
        B0 --- B1 --- B2 --- B3 --- B4 --- B5 --- B6 --- B7 --- B8 --- B9 --- B10 --- B11 --- B12
    end

    subgraph TAPE["Document Tape"]
        direction LR
        TN0["tape[0]\nOBJ_START\njump → 4"]
        TN1["tape[1]\nKEY\nsv { &buf[2], len=4 }"]
        TN2["tape[2]\nSTRING\nsv { &buf[9], len=3 }"]
        TN3["tape[3]\nOBJ_END\njump → 0"]
        TN0 --- TN1 --- TN2 --- TN3
    end

    TN1 -->|"string_view points\nto 'name' in buffer\n(zero copy)"| B2
    TN2 -->|"string_view points\nto 'Bob' in buffer\n(zero copy)"| B9
```

Accessing `root["name"]` returns a `string_view` pointing at `buf[2]` with `len=4`. **Zero bytes are allocated, zero bytes are copied.** The string is valid as long as the input buffer and `Document` are both alive.

### Contrast with `nlohmann/json`

```mermaid
flowchart LR
    subgraph NLO["nlohmann::json — string storage"]
        direction TB
        N1["input buffer: '...name...'"]
        N2["malloc(4) for 'name'\n(new std::string on the heap)"]
        N3["malloc(3) for 'Bob'\n(new std::string on the heap)"]
        N4["... × N for every string in document"]
        N1 --> N2 & N3 & N4
    end

    subgraph BEAST["beast::json — string storage"]
        direction TB
        B1["input buffer: '...name...'"]
        B2["string_view { &buf[2], 4 }\n(8-byte handle, no heap)"]
        B3["string_view { &buf[9], 3 }\n(8-byte handle, no heap)"]
        B1 --> B2 & B3
    end
```

---

## Layer 3: Stream-Push Serialization

Traditional serializers construct an intermediate in-memory JSON tree, then walk it to produce the output string. This doubles memory usage and adds a full O(N) pass before any byte is written.

Beast JSON uses a **stream-push model**: it walks your data structure once and writes tokens directly into the output buffer with no intermediate representation:

```mermaid
flowchart LR
    subgraph OLD["Traditional serializer — two-phase"]
        direction TB
        D1["User data\n(your structs)"]
        D2["Intermediate JSON tree\n(heap allocated — O(N) nodes)"]
        D3["Output string\n(second O(N) pass)"]
        D1 -->|"phase 1: build tree\n(N mallocs)"| D2
        D2 -->|"phase 2: stringify\n(1 malloc + memcpy)"| D3
    end

    subgraph NEW["Beast JSON — stream-push"]
        direction TB
        E1["User data\n(your structs)"]
        E2["Output buffer\n(caller pre-allocated)"]
        E1 -->|"single pass: write tokens directly\n(zero intermediate copies)"| E2
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

For real-time systems, the **tail latency** matters more than the average. Heap allocations cause unpredictable spikes:

```mermaid
flowchart LR
    subgraph NLO2["nlohmann::json — latency profile"]
        direction TB
        NP50["p50:   6 μs"]
        NP99["p99:  47 μs  ← GC / malloc pressure"]
        NP999["p99.9: 312 μs  ← OS page fault / lock contention"]
        NP50 --- NP99 --- NP999
    end

    subgraph BEAST2["Beast JSON — latency profile"]
        direction TB
        BP50["p50:  0.3 μs"]
        BP99["p99:  0.4 μs  ← near-deterministic"]
        BP999["p99.9: 0.5 μs  ← no allocator, no jitter"]
        BP50 --- BP99 --- BP999
    end
```

For systems with a 1 μs parse budget (co-located HFT, kernel-bypass networking, FPGA gateway), the difference between `nlohmann` and Beast JSON is not "faster" — it is the difference between **viable** and **not viable**.
