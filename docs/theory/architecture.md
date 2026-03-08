# The Tape Architecture

Beast JSON is built on a **Linear Tape DOM** — a design that fundamentally rejects the conventional tree-of-heap-nodes approach. Every JSON element maps to exactly one 64-bit `TapeNode` written into a single contiguous array. There is **one allocation, one pass, and zero pointer indirection** on the hot path.

---

## Why Conventional Parsers Are Slow

A tree-based DOM (e.g., `nlohmann/json`) allocates one heap node per JSON element. For a document with 10,000 elements, that means 10,000 `malloc` calls and 10,000 scattered heap objects — guaranteed cache misses on every traversal.

```mermaid
flowchart TB
    subgraph TREE["Tree DOM — scattered heap layout"]
        direction TB
        ROOT["Object node (heap malloc 1)<br/>ptr → children list"]
        N1["String node (heap malloc 2)<br/>'Alice' copy (heap malloc 3)"]
        N2["Int node (heap malloc 4)<br/>value: 30"]
        ROOT --> N1 --> N2
    end

    subgraph TAPE["Tape DOM — one contiguous array"]
        direction TB
        T0["[0] OBJ_START"]
        T1["[1] KEY 'name'"]
        T2["[2] STRING 'Alice'"]
        T3["[3] UINT64 30"]
        T4["[4] OBJ_END"]
        T0 --- T1 --- T2 --- T3 --- T4
    end
```

| | Tree DOM | Tape DOM |
|:---|:---|:---|
| Allocations per document | N (one per element) | **1** |
| Memory layout | Scattered heap objects | **Contiguous array** |
| Cache behavior | Pointer chase on every access | **Sequential scan** |
| String storage | Heap-copied `std::string` | **Zero-copy `string_view`** |
| Object skip | O(N) traversal | **O(1) via jump index** |

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
    T2["tape[2] — UINT64 — 101"]
    T3["tape[3] — KEY — 'active'"]
    T4["tape[4] — BOOL_TRUE"]
    T5["tape[5] — OBJ_END — jump: 0"]

    T0 --- T1 --- T2 --- T3 --- T4 --- T5
    T0 ---|"O(1) skip ↕"| T5
```

Reading this diagram:
- `tape[0]` stores `5` in its payload — the index of the matching `OBJ_END`. Skipping the entire object is a single array read: `tape[tape[0].jump]`.
- `tape[1]` and `tape[3]` (KEY) store a `string_view` pointing into the original input buffer. No allocation, no copy.
- `tape[2]` (UINT64) stores the integer `101` directly in the 56-bit payload field.
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
    subgraph BUF["Input Buffer — caller-owned, read-only"]
        RAW["{ #quot;name#quot;: #quot;Bob#quot; }"]
    end

    subgraph TAPE["Document Tape"]
        TN1["tape[1] KEY<br/>string_view { &amp;buf[2], len=4 } → 'name'"]
        TN2["tape[2] STRING<br/>string_view { &amp;buf[9], len=3 } → 'Bob'"]
    end

    TN1 -->|"zero-copy pointer"| BUF
    TN2 -->|"zero-copy pointer"| BUF
```

`string_view` lifetime: valid as long as both the `Document` and the input buffer are alive. The input buffer must not be modified or freed while any `Value` referencing it exists.

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

Stage 1 runs at near-memory-bandwidth speed by processing 64 bytes per instruction. Stage 2 only visits structural positions (5–15% of the input), making it branch-prediction-friendly and cache-hot.

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

## Why This Beats Tree-Based DOMs

| Metric | Beast JSON Tape | nlohmann/json | simdjson |
|:---|:---|:---|:---|
| **Memory layout** | Contiguous array | Scattered heap | Tape (no serialization) |
| **Allocations per parse** | 1 (tape itself) | O(N elements) | 2 (tape + strings) |
| **String storage** | Zero-copy `string_view` | Heap-copied `std::string` | Zero-copy `string_view` |
| **Object skip** | O(1) jump | O(N) recursion | O(1) jump |
| **Serialize support** | Yes (Stream-Push) | Yes | No (read-only DOM) |
| **Cache misses / element** | ~0 (sequential) | 1–3 (pointer chase) | ~0 (sequential) |
| **Peak RSS (twitter.json)** | 3.4 MB | 27.4 MB | 11.0 MB |
