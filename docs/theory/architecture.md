# The Lazy Tape DOM Architecture

Beast JSON's **Lazy Tape DOM** is an original parsing architecture designed from first principles to solve three specific problems that exist in every conventional JSON parser. It is not an incremental improvement — it is a different model of what a DOM should be.

This page explains:
- **The problems** that motivated the design
- **The theory** behind the two-part Lazy Tape model
- **The mechanisms** that implement it
- **The payoff** — which problem each decision solves

---

## The Problem: What Conventional Parsers Get Wrong

Every mainstream JSON parser makes the same three architectural mistakes:

### Problem 1 — Heap Scatter

Tree-based parsers (`nlohmann/json`, `RapidJSON`) allocate one heap node per element.
A 10,000-element document triggers 10,000 `malloc` calls. Each node lands at a
random address. Every access is a pointer chase, every pointer chase is a cache miss.

```
Input: { "a": 1, "b": 2, "c": 3 }

nlohmann/json memory layout:
  [heap addr 0x5a40] Object node  → ptr → [0x7f23] children vector
  [heap addr 0x7f23] "a"          → ptr → [0x3c11] IntNode(1)
  [heap addr 0x3c11] IntNode(1)
  [heap addr 0x9d04] "b"          → ptr → [0x12ef] IntNode(2)
  ...scattered across 8 different cache lines for 3 elements
```

### Problem 2 — Eager String Copy

Most parsers immediately allocate and copy every string value at parse time —
even strings the caller never reads. A 50 KB JSON payload with 200 string values
produces 200 separate heap allocations, whether you read 1 of them or all 200.

### Problem 3 — No Skip Mechanism

Conventional DOMs give every element equal weight. To find key `"z"` in an object,
the parser must walk past every sibling. To skip a nested array with 5,000 elements,
it must visit all 5,000. There is no way to say "skip this subtree in O(1)".

---

## The Lazy Tape DOM: The Two-Principle Solution

Beast JSON resolves all three problems with a single unified design built on two
inseparable principles:

```
┌──────────────────────────────────────────────────────────────────┐
│                     LAZY TAPE DOM                                │
│                                                                  │
│  ┌─────────────────────────┐   ┌──────────────────────────────┐  │
│  │         TAPE            │   │           LAZY               │  │
│  │                         │   │                              │  │
│  │  All nodes in one flat  │   │  Value = handle only.        │  │
│  │  contiguous array.      │   │  No data extracted until     │  │
│  │  One malloc. Zero       │   │  you call .as<T>().          │  │
│  │  pointer chasing.       │   │  Navigate costs nothing.     │  │
│  │  Jump indices for O(1)  │   │  Extract costs one array     │  │
│  │  subtree skip.          │   │  read.                       │  │
│  └─────────────────────────┘   └──────────────────────────────┘  │
│       solves Problem 1 & 3          solves Problem 2             │
└──────────────────────────────────────────────────────────────────┘
```

The namespace `beast::json::lazy` directly names this principle.

---

## DocumentView: The Single Allocation

Parsing produces a `DocumentView` — a single heap object that owns everything.

```mermaid
flowchart TB
    subgraph INPUT["Caller-owned input buffer (never copied)"]
        RAW["{ #quot;user#quot;: { #quot;name#quot;: #quot;Alice#quot;, #quot;age#quot;: 30 } }"]
    end

    subgraph DOC["beast::Document  (= beast::json::lazy::DocumentView)"]
        direction TB
        TAPE["TapeArena<br/>─────────────────────────────<br/>tape[0]  OBJ_START  jump→8<br/>tape[1]  KEY        →'user'<br/>tape[2]  OBJ_START  jump→7<br/>tape[3]  KEY        →'name'<br/>tape[4]  STRING     →'Alice'<br/>tape[5]  KEY        →'age'<br/>tape[6]  INT64      30<br/>tape[7]  OBJ_END    jump→2<br/>tape[8]  OBJ_END    jump→0<br/>─────────────────────────────<br/>one contiguous allocation"]
        IDX["Stage1Index<br/>structural positions<br/>(reused across parses)"]
        SRC["string_view<br/>→ points into INPUT<br/>(zero copy)"]
    end

    INPUT -->|"string_view (not copied)"| SRC
    SRC -.->|"KEY/STRING nodes point back"| INPUT
```

- `TapeArena` is a single `malloc`. On repeated parses it is **reused in-place** (reset cursor, keep capacity).
- `Stage1Index` is the SIMD-produced structural position list. Also reused without reallocation.
- The input buffer is **never copied**. `string_view` pointers go directly into the caller's memory.

---

## Value: The Lazy Handle

`beast::Value` holds exactly two fields — 16 bytes total:

```
struct Value {          // 16 bytes
    DocumentView* doc_; //  8 bytes — which document
    uint32_t      idx_; //  4 bytes — which tape slot
    uint32_t      pad_; //  4 bytes — alignment
};
```

This is the "lazy" half of the design. A `Value` is not a value — it is a **position**.

### The Three-Phase Lifecycle

```mermaid
flowchart LR
    subgraph PHASE1["① PARSE  (eager, once)"]
        direction TB
        P1["SIMD Stage 1:<br/>scan 64 bytes/cycle<br/>find structural chars"]
        P2["Stage 2:<br/>write TapeNodes<br/>into TapeArena"]
        P1 --> P2
    end

    subgraph PHASE2["② NAVIGATE  (zero cost)"]
        direction TB
        N1["root[#quot;user#quot;]<br/>→ walks tape keys<br/>returns Value{doc, 2}"]
        N2["root[#quot;user#quot;][#quot;name#quot;]<br/>→ jumps into sub-object<br/>returns Value{doc, 4}"]
        N3["No memory read<br/>beyond TapeNode.meta<br/>No allocation ever"]
        N1 --> N2 --> N3
    end

    subgraph PHASE3["③ EXTRACT  (lazy, on demand)"]
        direction TB
        E1[".as&lt;std::string_view&gt;()<br/>reads tape[4].offset<br/>returns string_view into input"]
        E2[".as&lt;int&gt;()<br/>reads tape[6].meta<br/>returns inline integer"]
        E3["one array read<br/>zero allocation<br/>zero copy"]
        E1 & E2 --> E3
    end

    PHASE1 -->|"Document ready"| PHASE2
    PHASE2 -->|"Value handle"| PHASE3
```

**The key insight:** the caller controls when extraction happens.
If you navigate to `root["user"]["name"]` and never call `.as<>()`,
the string bytes are never touched. This is qualitatively different from
every eager parser — you pay only for what you read.

---

## Why Conventional Parsers Are Slow

A tree-based DOM allocates one heap node per JSON element. For a document with 10,000 elements, that means 10,000 `malloc` calls and 10,000 scattered heap objects — guaranteed cache misses on every traversal.

```mermaid
flowchart TB
    subgraph TREE["Tree DOM — scattered heap layout"]
        direction TB
        ROOT["Object node (heap malloc 1)<br/>ptr → children list"]
        N1["String node (heap malloc 2)<br/>'Alice' copy (heap malloc 3)"]
        N2["Int node (heap malloc 4)<br/>value: 30"]
        ROOT --> N1 --> N2
    end

    subgraph TAPE["Lazy Tape DOM — one contiguous array"]
        direction TB
        T0["[0] OBJ_START"]
        T1["[1] KEY 'name'"]
        T2["[2] STRING → input buffer (zero copy)"]
        T3["[3] INT64 30 (inline)"]
        T4["[4] OBJ_END"]
        T0 --- T1 --- T2 --- T3 --- T4
    end
```

| | Tree DOM | Lazy Tape DOM |
|:---|:---|:---|
| Allocations per document | N (one per element) | **1** |
| Memory layout | Scattered heap objects | **Contiguous array** |
| Cache behavior | Pointer chase on every access | **Sequential scan** |
| String storage | Heap-copied `std::string` | **Zero-copy `string_view`** |
| Object skip | O(N) traversal | **O(1) via jump index** |
| Extraction cost | Paid at parse time (always) | **Paid at `.as<T>()` (on demand)** |

---

## Memory Layout: The Linear Tape

Given this input:

```json
{ "id": 101, "active": true }
```

Beast JSON performs one pass and writes 6 sequential 8-byte slots:

```mermaid
flowchart TB
    JSON["{ #quot;id#quot;: 101, #quot;active#quot;: true }"]
    JSON -->|"single-pass parse"| T0

    T0["tape[0] — OBJ_START — jump: 5"]
    T1["tape[1] — KEY — 'id'"]
    T2["tape[2] — INT64 — 101"]
    T3["tape[3] — KEY — 'active'"]
    T4["tape[4] — BOOL_TRUE"]
    T5["tape[5] — OBJ_END — jump: 0"]

    T0 --- T1 --- T2 --- T3 --- T4 --- T5
    T0 ---|"O(1) skip ↕"| T5
```

Reading this diagram:
- `tape[0]` stores `5` in its payload — the index of the matching `OBJ_END`. Skipping the entire object is a single array read: `tape[tape[0].jump]`.
- `tape[1]` and `tape[3]` (KEY) store a `string_view` pointing into the original input buffer. No allocation, no copy.
- `tape[2]` (INT64) stores `101` directly in the 56-bit payload field. No heap involved.
- `tape[4]` (BOOL_TRUE) needs only the type tag — payload is unused.

---

## TapeNode: 64-Bit Encoding

Every element — object, array, string, integer, float, bool, null — is encoded in exactly **8 bytes**:

```mermaid
flowchart LR
    subgraph NODE["TapeNode — 64 bits (8 bytes)"]
        direction LR
        TAG["bits 63–56<br/>────────<br/>Type Tag<br/>(8 bits)"]
        PAY["bits 55–0<br/>────────<br/>Payload<br/>(56 bits)"]
        TAG --- PAY
    end
```

**Type tag values:**

| Tag | Name | Payload meaning |
|:---|:---|:---|
| `0x01` | `OBJ_START` | Index of matching `OBJ_END` |
| `0x02` | `OBJ_END` | Index of matching `OBJ_START` |
| `0x03` | `ARR_START` | Index of matching `ARR_END` |
| `0x04` | `ARR_END` | Index of matching `ARR_START` |
| `0x05` | `KEY` | `ptr` (48-bit) + `len` (8-bit) into input buffer |
| `0x06` | `STRING` | `ptr` (48-bit) + `len` (8-bit) into input buffer |
| `0x07` | `UINT64` | Value stored inline (up to 2⁵⁶ − 1) |
| `0x08` | `INT64` | Value stored inline (sign-extended) |
| `0x09` | `DOUBLE` | Bit-cast from `double` |
| `0x0A` | `BOOL_TRUE` | Unused |
| `0x0B` | `BOOL_FALSE` | Unused |
| `0x0C` | `NULL_VAL` | Unused |

The 8-bit type tag is handled by a branch-predictor-friendly `switch`. The 56-bit payload accommodates a 48-bit virtual address plus an 8-bit length hint — enough for a `string_view` with no heap involved.

---

## Zero-Copy String Model

String data is **never copied**. KEY and STRING nodes store a `string_view` pointing directly into the caller's input buffer:

```mermaid
flowchart TB
    subgraph BUF["Input Buffer — caller-owned, never touched by the parser"]
        RAW["{ #quot;name#quot;: #quot;Bob#quot; }"]
    end

    subgraph TAPE["Document Tape (TapeArena)"]
        TN1["tape[1] KEY<br/>string_view { &amp;buf[2], len=4 } → 'name'"]
        TN2["tape[2] STRING<br/>string_view { &amp;buf[9], len=3 } → 'Bob'"]
    end

    subgraph VAL["beast::Value  (lazy handle)"]
        V["doc=&amp;document, idx=2<br/>— nothing extracted yet —"]
    end

    TN1 -->|"zero-copy pointer"| BUF
    TN2 -->|"zero-copy pointer"| BUF
    VAL -->|".as&lt;string_view&gt;() reads TapeNode → returns string_view into BUF"| TN2
```

`string_view` lifetime: valid as long as both the `Document` and the input buffer are alive.
The input buffer must not be modified or freed while any `Value` referencing it exists.

---

## Multi-Stage SIMD Pipeline

Parsing runs in two tightly coupled stages across the same input buffer:

```mermaid
flowchart TB
    RAW["Raw JSON bytes"]

    subgraph S1["Stage 1 — Structural Indexing (SIMD)"]
        direction TB
        AVX["AVX-512<br/>64 bytes/cycle (Intel Ice Lake+)"]
        NEON["NEON<br/>16 bytes/cycle (ARM / Apple Silicon)"]
        BITS["Structural Bitset<br/>one bit per input byte<br/>1 = structural char, 0 = inside string or data"]
        AVX --> BITS
        NEON --> BITS
    end

    subgraph S2["Stage 2 — Tape Generation (scalar)"]
        direction TB
        WALK["Walk bitset — iterate set bits only"]
        STR["Strings → string_view (zero copy)"]
        NUM["Numbers → Russ Cox algorithm (no strtod)"]
        BOOL["Bool / Null → type tag only"]
        OUT["tape[] — TapeNode array, ready for query"]
        WALK --> STR & NUM & BOOL --> OUT
    end

    RAW --> S1
    S1 -->|"sparse bitset"| S2
```

Stage 1 runs at near-memory-bandwidth speed by processing 64 bytes per instruction.
Stage 2 only visits structural positions (5–15% of the input), making it branch-prediction-friendly and cache-hot.

---

## Object and Array Traversal

Jump pointers in `OBJ_START` / `OBJ_END` and `ARR_START` / `ARR_END` enable sub-linear traversal:

```mermaid
flowchart TB
    subgraph DOC["Tape for: { a: [1,2,3], b: true }"]
        direction TB
        I0["[0] OBJ_START — jump: 9"]
        I1["[1] KEY 'a'"]
        I2["[2] ARR_START — jump: 6"]
        I3["[3] UINT64 1"]
        I4["[4] UINT64 2"]
        I5["[5] UINT64 3"]
        I6["[6] ARR_END — jump: 2"]
        I7["[7] KEY 'b'"]
        I8["[8] BOOL_TRUE"]
        I9["[9] OBJ_END — jump: 0"]
        I0 --- I1 --- I2 --- I3 --- I4 --- I5 --- I6 --- I7 --- I8 --- I9
    end

    I2 ---|"O(1) skip array ↕"| I6
    I0 ---|"O(1) skip object ↕"| I9
```

Use case: querying only key `"b"` in an object with a huge nested array under `"a"`. The parser jumps from `ARR_START` directly to `ARR_END` in one step — O(1) regardless of array size.

---

## Design Principles → Problems Solved

Every decision in the Lazy Tape DOM traces directly to one of the three original problems:

| Design decision | Solves | How |
|:---|:---|:---|
| One contiguous `TapeArena` | Problem 1 — Heap Scatter | Single `malloc`, sequential layout, zero pointer chasing |
| 8-byte fixed-size `TapeNode` | Problem 1 — Heap Scatter | CPU cache line holds 8 nodes; prefetcher works perfectly |
| `OBJ_START`/`ARR_START` jump index | Problem 3 — No Skip | Subtree skip is one integer read, O(1) regardless of depth or size |
| `string_view` into input buffer | Problem 2 — Eager Copy | Zero bytes copied at parse time; string is only touched if `.as<string_view>()` is called |
| `Value` = 16-byte handle `{doc*, idx}` | Problem 2 — Eager Copy | Navigation produces no data, no allocation; extraction is opt-in |
| SIMD Stage 1 structural scan | Problem 1 — Heap Scatter | Structural chars identified at memory-bandwidth speed before any allocation |
| `Stage1Index` reuse across parses | Problem 1 — Heap Scatter | Hot-loop parsing (JSON streams) reuses both tape and index without any `malloc` |

Taken together:

```
You pay the cost of parsing exactly once.
You pay the cost of navigation never.
You pay the cost of extraction only for the fields you actually read.
```

---

## Why This Beats Tree-Based DOMs

| Metric | Beast JSON (Lazy Tape) | nlohmann/json | simdjson |
|:---|:---|:---|:---|
| **Memory layout** | Contiguous array | Scattered heap | Tape (read-only) |
| **Allocations per parse** | 1 (tape itself) | O(N elements) | 2 (tape + strings) |
| **String storage** | Zero-copy `string_view` | Heap-copied `std::string` | Zero-copy `string_view` |
| **Object/array skip** | O(1) jump | O(N) recursion | O(1) jump |
| **Extraction model** | Lazy — on demand | Eager — at parse time | Lazy — on demand |
| **Mutation support** | Yes (overlay map) | Yes (in-place) | No (read-only DOM) |
| **Serialize support** | Yes | Yes | No |
| **Cache misses / element** | ~0 (sequential) | 1–3 (pointer chase) | ~0 (sequential) |
| **Peak RSS (twitter.json)** | 3.4 MB | 27.4 MB | 11.0 MB |
