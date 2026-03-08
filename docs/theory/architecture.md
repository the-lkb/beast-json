# The Tape Architecture

Beast JSON is built on a **Linear Tape DOM** — a design that fundamentally rejects the conventional tree-of-heap-nodes approach. Every JSON element maps to exactly one 64-bit `TapeNode` written into a single contiguous array. There is **one allocation, one pass, and zero pointer indirection** on the hot path.

---

## Why Conventional Parsers Are Slow

A tree-based DOM (e.g., `nlohmann/json`) allocates one heap node per JSON element. For a document with 10,000 elements, that means 10,000 `malloc` calls and 10,000 scattered heap objects — guaranteed cache misses on every traversal.

```mermaid
flowchart TB
    subgraph TREE["Tree DOM — pointer-chasing heap layout"]
        direction TB
        ROOT["root Object\n(heap addr: 0x5f20)\n┌──────────────────┐\n│ children: ptr────┼──┐\n│ next: ptr        │  │\n└──────────────────┘  │\n                      ▼"]
        N1["name: String\n(heap addr: 0x6a40)\n┌──────────────────┐\n│ data: ptr────────┼──► 'Alice' (heap copy)\n│ next: ptr────────┼──┐\n└──────────────────┘  │\n                      ▼"]
        N2["age: Int\n(heap addr: 0x7b80)\n┌──────────────────┐\n│ value: 30        │\n│ next: nullptr    │\n└──────────────────┘"]
        ROOT --> N1 --> N2
    end

    subgraph TAPE["Tape DOM — sequential array in one allocation"]
        direction LR
        T0["tape[0]\nOBJ_START\njump=4"]
        T1["tape[1]\nKEY\nview='name'"]
        T2["tape[2]\nSTRING\nview='Alice'"]
        T3["tape[3]\nUINT64\n30"]
        T4["tape[4]\nOBJ_END\njump=0"]
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

Beast JSON performs one pass and writes this array (indices 0–5 are sequential memory addresses):

```mermaid
flowchart LR
    JSON["JSON Input\n{ id: 101, active: true }"]

    subgraph TAPE["tape[]  —  6 × 8 bytes = 48 bytes, contiguous in memory"]
        direction LR
        T0["tape[0]\n─────────\nOBJ_START\njump → 5"]
        T1["tape[1]\n─────────\nKEY\nview 'id'"]
        T2["tape[2]\n─────────\nUINT64\n101"]
        T3["tape[3]\n─────────\nKEY\nview 'active'"]
        T4["tape[4]\n─────────\nBOOL_TRUE\n—"]
        T5["tape[5]\n─────────\nOBJ_END\njump → 0"]
        T0 --- T1 --- T2 --- T3 --- T4 --- T5
    end

    T0 <-->|"O(1) skip\n(jump pointer)"| T5
    JSON -->|"single-pass parse"| T0
```

Reading this diagram:
- `tape[0]` (OBJ_START) stores `5` in its payload — the index of the matching `OBJ_END`. Skipping the entire object means reading `tape[tape[0].jump]` — one array access.
- `tape[1]` and `tape[3]` (KEY) store a `string_view` pointing into the original input buffer. No allocation, no copy.
- `tape[2]` (UINT64) stores the integer `101` directly in the 56-bit payload.
- `tape[4]` (BOOL_TRUE) needs only the type tag. Payload is unused.

---

## TapeNode: 64-Bit Encoding

Every element — object, array, string, integer, float, bool, null — is encoded in exactly **8 bytes**:

```mermaid
flowchart LR
    subgraph NODE["TapeNode  —  64 bits (8 bytes)"]
        direction LR
        TAG["bits 63–56\n────────────\nType Tag  (8 bits)\n\n0x01  OBJ_START\n0x02  OBJ_END\n0x03  ARR_START\n0x04  ARR_END\n0x05  KEY\n0x06  STRING\n0x07  UINT64\n0x08  INT64\n0x09  DOUBLE\n0x0A  BOOL_TRUE\n0x0B  BOOL_FALSE\n0x0C  NULL_VAL"]
        PAY["bits 55–0\n────────────\nPayload  (56 bits)\n\nOBJ/ARR START:\n  jump = end index\nOBJ/ARR END:\n  jump = start index\nKEY/STRING:\n  ptr = &input[i]\n  len = byte length\nUINT64/INT64:\n  value (inline)\nDOUBLE:\n  bit-cast from double\nBOOL/NULL:\n  unused"]
        TAG --- PAY
    end
```

The 8-bit type tag fits in one byte, leaving 56 bits for the payload. On 64-bit systems, a virtual address only uses 48 bits (or 57 bits with 5-level paging), so a pointer plus an 8-bit length hint fits in the payload for short strings — no heap involved.

---

## Zero-Copy String Model

String data is **never copied**. KEY and STRING nodes store a `string_view` whose `.data()` points directly into the caller's input buffer:

```mermaid
flowchart TB
    subgraph BUF["Input Buffer  —  caller-owned, read-only"]
        direction LR
        B0["[0]\n'{'"]
        B1["[1]\n'\"'"]
        B2["[2]\n'i'"]
        B3["[3]\n'd'"]
        B4["[4]\n'\"'"]
        B5["[5]\n':'"]
        B6["[6]\n'1'"]
        B7["[7]\n'0'"]
        B8["[8]\n'1'"]
        B9["[9]\n','"]
        B0 --- B1 --- B2 --- B3 --- B4 --- B5 --- B6 --- B7 --- B8 --- B9
    end

    subgraph TN1["tape[1]  KEY"]
        K["string_view\ndata = &buf[2]\nlen  = 2\n→ 'id'"]
    end

    subgraph TN2["tape[2]  UINT64"]
        V["payload\n= 101\n(stored inline)"]
    end

    K -->|"points into\noriginal buffer\n(zero copy)"| B2
    V -.- B6
```

`string_view` lifetime: valid as long as both the `Document` and the input buffer are alive. The input buffer must not be modified or freed while any `Value` referencing it exists.

---

## Multi-Stage SIMD Pipeline

Parsing runs in two tightly coupled stages across the same input buffer:

```mermaid
flowchart LR
    RAW["Raw JSON\nbytes"]

    subgraph S1["Stage 1 — Structural Indexing (SIMD)"]
        direction TB
        AVX["AVX-512\n64 bytes/cycle\n(Intel Ice Lake+)"]
        NEON["NEON\n16 bytes/cycle\n(ARM / Apple Silicon)"]
        BITS["Structural Bitset\nOne bit per input byte\n1 = structural character\n0 = data / inside string"]
        AVX --> BITS
        NEON --> BITS
    end

    subgraph S2["Stage 2 — Tape Generation (scalar)"]
        direction TB
        WALK["Walk bitset\n(iterate set bits only)"]
        STR["Strings\nstring_view into buf\n(zero copy)"]
        NUM["Numbers\nRuss Cox algorithm\n(no strtod)"]
        BOOL["Bool / Null\nType tag only"]
        OUT["tape[]\nTapeNode array\nready for query"]
        WALK --> STR & NUM & BOOL --> OUT
    end

    RAW --> S1
    S1 -->|"sparse bitset\n(most bits = 0)"| S2
```

Stage 1 is the bandwidth-bound phase — it runs at near-memory-bandwidth speed by processing 64 bytes per instruction. Stage 2 only visits structural positions (a small fraction of the total input), making it branch-prediction-friendly and cache-hot.

---

## Object and Array Traversal

The jump pointers in OBJ_START / OBJ_END (and ARR_START / ARR_END) enable sub-linear traversal:

```mermaid
flowchart LR
    subgraph DOC["Tape for: { a: [1,2,3], b: true }"]
        direction LR
        I0["[0] OBJ_START\njump=10"]
        I1["[1] KEY 'a'"]
        I2["[2] ARR_START\njump=6"]
        I3["[3] UINT64 1"]
        I4["[4] UINT64 2"]
        I5["[5] UINT64 3"]
        I6["[6] ARR_END\njump=2"]
        I7["[7] KEY 'b'"]
        I8["[8] BOOL_TRUE"]
        I9["[9] OBJ_END\njump=0"]
        I0 --- I1 --- I2 --- I3 --- I4 --- I5 --- I6 --- I7 --- I8 --- I9
    end

    I2 <-->|"skip array\ntape[jump=6]"| I6
    I0 <-->|"skip object\ntape[jump=9]"| I9
```

Use case: querying only key `"b"` in a large object with a huge nested array under `"a"`. With jump pointers, the parser jumps from `ARR_START` directly to `ARR_END` in one step — O(1) regardless of array size.

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
