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

<div class="bd-principle-pair">
  <div class="bd-principle-header">LAZY TAPE DOM</div>
  <div class="bd-principle bd-principle--left">
    <div class="bd-principle__title">TAPE</div>
    <ul class="bd-principle__points">
      <li>All nodes in one flat contiguous array</li>
      <li>One malloc. Zero pointer chasing.</li>
      <li>Jump indices for O(1) subtree skip</li>
    </ul>
    <div class="bd-principle__solves">solves Problem 1 &amp; 3</div>
  </div>
  <div class="bd-principle">
    <div class="bd-principle__title">LAZY</div>
    <ul class="bd-principle__points">
      <li>Value = handle only</li>
      <li>No data extracted until you call .as&lt;T&gt;()</li>
      <li>Navigate costs nothing</li>
      <li>Extract costs one array read</li>
    </ul>
    <div class="bd-principle__solves">solves Problem 2</div>
  </div>
</div>

The namespace `beast::json::lazy` directly names this principle.

---

## DocumentView: The Single Allocation

Parsing produces a `DocumentView` — a single heap object that owns everything.

<TapeFlowDiagram />

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

<LazyLifecycle />

**The key insight:** the caller controls when extraction happens.
If you navigate to `root["user"]["name"]` and never call `.as<>()`,
the string bytes are never touched. This is qualitatively different from
every eager parser — you pay only for what you read.

---

## Why Conventional Parsers Are Slow

A tree-based DOM allocates one heap node per JSON element. For a document with 10,000 elements, that means 10,000 `malloc` calls and 10,000 scattered heap objects — guaranteed cache misses on every traversal.

<TreeVsTape />

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

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand" style="max-width:340px;">{ &quot;id&quot;: 101, &quot;active&quot;: true }</div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">single-pass SIMD parse</div></div>
    <div class="bd-tape-strip">
      <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[0]</span><span class="bd-tape-cell__tag">OBJ_START</span><span class="bd-tape-cell__val">jump: 5</span></div>
      <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[1]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">"id"</span></div>
      <div class="bd-tape-cell bd-tape-cell--int"><span class="bd-tape-cell__idx">tape[2]</span><span class="bd-tape-cell__tag">INT64</span><span class="bd-tape-cell__val">101</span></div>
      <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[3]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">"active"</span></div>
      <div class="bd-tape-cell bd-tape-cell--bool"><span class="bd-tape-cell__idx">tape[4]</span><span class="bd-tape-cell__tag">BOOL_TRUE</span><span class="bd-tape-cell__val">—</span></div>
      <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[5]</span><span class="bd-tape-cell__tag">OBJ_END</span><span class="bd-tape-cell__val">jump: 0</span></div>
    </div>
    <div class="bd-callout" style="font-size:0.8rem;margin:0.5rem 0 0;">
      <strong>O(1) skip:</strong> <code>tape[tape[0].jump]</code> → jumps from tape[0] to tape[5] in one array read
    </div>
  </div>
</div>

Reading this diagram:
- `tape[0]` stores `5` in its payload — the index of the matching `OBJ_END`. Skipping the entire object is a single array read: `tape[tape[0].jump]`.
- `tape[1]` and `tape[3]` (KEY) store a `string_view` pointing into the original input buffer. No allocation, no copy.
- `tape[2]` (INT64) stores `101` directly in the 56-bit payload field. No heap involved.
- `tape[4]` (BOOL_TRUE) needs only the type tag — payload is unused.

---

## TapeNode: 64-Bit Encoding

Every element — object, array, string, integer, float, bool, null — is encoded in exactly **8 bytes**:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-bits">
      <div class="bd-bit-seg" style="width:90px;flex-shrink:0;background:color-mix(in srgb,var(--vp-c-brand-1) 20%,transparent);border-radius:4px 0 0 4px;">
        <span class="bd-bit-seg__range">bits 63–56</span>
        <span class="bd-bit-seg__val">Type Tag</span>
        <span class="bd-bit-seg__name">(8 bits)</span>
      </div>
      <div class="bd-bit-seg" style="flex:1;background:color-mix(in srgb,var(--vp-c-brand-1) 9%,transparent);border:1px solid var(--vp-c-divider);border-radius:0 4px 4px 0;">
        <span class="bd-bit-seg__range">bits 55–0</span>
        <span class="bd-bit-seg__val">Payload</span>
        <span class="bd-bit-seg__name">(56 bits)</span>
      </div>
    </div>
    <div class="bd-row" style="gap:2rem;margin-top:0.5rem;font-size:0.78rem;color:var(--vp-c-text-2);font-family:var(--vp-font-family-mono);">
      <span>0x01–0x0C = node type</span>
      <span>jump index / ptr+len / inline value</span>
    </div>
  </div>
</div>

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

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:480px;">
      <div class="bd-group__title">Input Buffer — caller-owned, never copied</div>
      <div class="bd-group__body">
        <div class="bd-box bd-box--blue" style="font-size:0.85rem;">{ &quot;name&quot;: &quot;Bob&quot; }</div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">zero-copy pointer</div></div>
    <div class="bd-group" style="width:100%;max-width:480px;">
      <div class="bd-group__title">Document Tape (TapeArena)</div>
      <div class="bd-group__body">
        <div class="bd-tape-strip" style="justify-content:center;">
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[1] KEY</span><span class="bd-tape-cell__tag">string_view</span><span class="bd-tape-cell__val">&amp;buf[2], len=4 → "name"</span></div>
          <div class="bd-tape-cell bd-tape-cell--str"><span class="bd-tape-cell__idx">tape[2] STRING</span><span class="bd-tape-cell__tag">string_view</span><span class="bd-tape-cell__val">&amp;buf[9], len=3 → "Bob"</span></div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">.as&lt;string_view&gt;() reads TapeNode → returns string_view into BUF</div></div>
    <div class="bd-group" style="width:100%;max-width:480px;">
      <div class="bd-group__title">beast::Value (lazy handle)</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.78rem;">doc=&amp;document, idx=2<br><small>— nothing extracted yet —</small></div>
      </div>
    </div>
  </div>
</div>

`string_view` lifetime: valid as long as both the `Document` and the input buffer are alive.
The input buffer must not be modified or freed while any `Value` referencing it exists.

---

## Multi-Stage SIMD Pipeline

Parsing runs in two tightly coupled stages across the same input buffer:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand">Raw JSON bytes</div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">Stage 1 — Structural Indexing (SIMD)</div>
      <div class="bd-group__body">
        <div class="bd-row">
          <div class="bd-box bd-box--teal">AVX-512<br><small>64 bytes/cycle (Intel Ice Lake+)</small></div>
          <div class="bd-box bd-box--teal">NEON<br><small>16 bytes/cycle (ARM / Apple Silicon)</small></div>
        </div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--brand" style="max-width:360px;">Structural Bitset<br><small>one bit per input byte — 1 = structural char, 0 = data</small></div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">sparse bitset</div></div>
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">Stage 2 — Tape Generation (scalar)</div>
      <div class="bd-group__body">
        <div class="bd-box" style="max-width:300px;">Walk bitset — iterate set bits only<br><small>(5–15% of input)</small></div>
        <div class="bd-row" style="gap:0.5rem;">
          <div class="bd-box bd-box--purple">Strings<br><small>→ string_view (zero copy)</small></div>
          <div class="bd-box bd-box--green">Numbers<br><small>→ Russ Cox (no strtod)</small></div>
          <div class="bd-box bd-box--orange">Bool/Null<br><small>→ type tag only</small></div>
        </div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--brand">tape[] — TapeNode array, ready for query</div>
      </div>
    </div>
  </div>
</div>

Stage 1 runs at near-memory-bandwidth speed by processing 64 bytes per instruction.
Stage 2 only visits structural positions (5–15% of the input), making it branch-prediction-friendly and cache-hot.

---

## Object and Array Traversal

Jump pointers in `OBJ_START` / `OBJ_END` and `ARR_START` / `ARR_END` enable sub-linear traversal:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;">
      <div class="bd-group__title">Tape for: { "a": [1, 2, 3], "b": true }</div>
      <div class="bd-group__body">
        <div class="bd-tape-strip">
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[0]</span><span class="bd-tape-cell__tag">OBJ_START</span><span class="bd-tape-cell__val">jump→9</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[1]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">"a"</span></div>
          <div class="bd-tape-cell bd-tape-cell--arr"><span class="bd-tape-cell__idx">tape[2]</span><span class="bd-tape-cell__tag">ARR_START</span><span class="bd-tape-cell__val">jump→6</span></div>
          <div class="bd-tape-cell bd-tape-cell--int"><span class="bd-tape-cell__idx">tape[3]</span><span class="bd-tape-cell__tag">UINT64</span><span class="bd-tape-cell__val">1</span></div>
          <div class="bd-tape-cell bd-tape-cell--int"><span class="bd-tape-cell__idx">tape[4]</span><span class="bd-tape-cell__tag">UINT64</span><span class="bd-tape-cell__val">2</span></div>
          <div class="bd-tape-cell bd-tape-cell--int"><span class="bd-tape-cell__idx">tape[5]</span><span class="bd-tape-cell__tag">UINT64</span><span class="bd-tape-cell__val">3</span></div>
          <div class="bd-tape-cell bd-tape-cell--arr"><span class="bd-tape-cell__idx">tape[6]</span><span class="bd-tape-cell__tag">ARR_END</span><span class="bd-tape-cell__val">jump→2</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[7]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">"b"</span></div>
          <div class="bd-tape-cell bd-tape-cell--bool"><span class="bd-tape-cell__idx">tape[8]</span><span class="bd-tape-cell__tag">BOOL_TRUE</span><span class="bd-tape-cell__val">—</span></div>
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[9]</span><span class="bd-tape-cell__tag">OBJ_END</span><span class="bd-tape-cell__val">jump→0</span></div>
        </div>
      </div>
    </div>
    <div class="bd-row" style="gap:1rem;">
      <div class="bd-callout bd-callout--green" style="flex:1;margin:0;font-size:0.78rem;">
        <strong>Skip array:</strong> tape[2].jump = 6 → jump from ARR_START to ARR_END in <strong>1 read (O(1))</strong>
      </div>
      <div class="bd-callout" style="flex:1;margin:0;font-size:0.78rem;">
        <strong>Skip object:</strong> tape[0].jump = 9 → jump from OBJ_START to OBJ_END in <strong>1 read (O(1))</strong>
      </div>
    </div>
  </div>
</div>

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

<div class="bd-callout bd-callout--green">
  <strong>You pay the cost of parsing exactly once.</strong><br>
  You pay the cost of navigation <strong>never</strong>.<br>
  You pay the cost of extraction <strong>only for the fields you actually read</strong>.
</div>

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
