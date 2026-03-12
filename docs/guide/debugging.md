# Debugging Guide

This guide explains exactly how qbuem-json reads and stores data at the core level, then shows you how to diagnose every category of error — from beginner mistakes to expert-level memory issues.

---

## Part 1 — How the Core Works

Understanding the data flow is the foundation of effective debugging. Every parse goes through the same deterministic pipeline.

### The Full Pipeline

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand">Your JSON string <small>caller-owned buffer — never copied</small></div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">Stage 1 — SIMD Structural Indexing</div>
      <div class="bd-group__body" style="align-items:flex-start;">
        <div class="bd-box bd-box--teal" style="max-width:100%;width:100%;text-align:left;">
          Load 64 bytes/cycle → zmm0 (AVX-512)<br>
          <small>8× VPCMPEQB → 64-bit structural_mask</small><br>
          <small>PCLMULQDQ prefix-XOR → suppress chars in strings</small><br>
          <small>Result: sparse bitset of structural positions</small>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">clean_structural_mask</div></div>
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">Stage 2 — Tape Generation (scalar)</div>
      <div class="bd-group__body" style="align-items:flex-start;">
        <div class="bd-box" style="max-width:100%;width:100%;text-align:left;">
          TZCNT iterate only structural positions (5–15%)<br>
          <small>Each position → one TapeNode written</small><br>
          <small>Strings → zero-copy string_view into input buf</small><br>
          <small>Numbers → Russ Cox inline value (no strtod)</small><br>
          <small>Jump patches resolved at OBJ_END / ARR_END</small>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">TapeArena filled — 1 malloc total</div></div>
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">DocumentView</div>
      <div class="bd-group__body" style="align-items:flex-start;">
        <div class="bd-box" style="max-width:100%;width:100%;text-align:left;">
          TapeArena* — owns all TapeNodes<br>
          <small>string_view — points into your input buffer</small><br>
          <small>Stage1Index — reused across calls, no extra alloc</small>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-box bd-box--brand"><code>qbuem::Value { doc*, idx }</code> <small>— 16-byte lazy handle</small></div>
  </div>
</div>

**Where errors can occur:**

| Stage | Typical errors |
|:---|:---|
| Stage 1 (SIMD) | Unterminated strings, malformed escape sequences |
| Stage 2 (scalar) | Unexpected tokens, number overflow, missing closing bracket |
| `.as<T>()` call | Type mismatch, null value access |
| After parse | Dangling `string_view`, `Document` destroyed too early |

---

## Part 2 — How Data is Stored

Every single element — object, array, string, integer, float, bool, null — occupies exactly **8 bytes** in the TapeArena. There are no pointers to external heap objects.

### The TapeNode: 64-bit Encoding

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
  </div>
</div>

Click any node below to see its exact encoding:

<TapeInspector />

### Type Tag Reference

| Tag | Name | Payload meaning | Access |
|:---|:---|:---|:---|
| `0x01` | `OBJ_START` | Index of matching `OBJ_END` | `is_object()` |
| `0x02` | `OBJ_END` | Index of matching `OBJ_START` | internal |
| `0x03` | `ARR_START` | Index of matching `ARR_END` | `is_array()` |
| `0x04` | `ARR_END` | Index of matching `ARR_START` | internal |
| `0x05` | `KEY` | 48-bit ptr + 8-bit len into input buf | key matching |
| `0x06` | `STRING` | 48-bit ptr + 8-bit len into input buf | `.as<string_view>()` |
| `0x07` | `UINT64` | Unsigned value inline (≤ 2⁵⁶−1) | `.as<uint64_t>()` |
| `0x08` | `INT64` | Signed value inline (sign-extended) | `.as<int64_t>()` |
| `0x09` | `DOUBLE` | IEEE 754 bit-cast, no `strtod` | `.as<double>()` |
| `0x0A` | `BOOL_TRUE` | Unused | `.as<bool>()` → true |
| `0x0B` | `BOOL_FALSE` | Unused | `.as<bool>()` → false |
| `0x0C` | `NULL_VAL` | Unused | `.is_null()` |

### String Storage: Zero-Copy

The most important thing to understand about qbuem-json strings:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">Input Buffer — your memory (stack, mmap, recv buffer)</div>
      <div class="bd-group__body">
        <div class="bd-box bd-box--blue" style="font-size:0.82rem;letter-spacing:0.08em;">{ &quot;name&quot; : &quot;Alice&quot; }</div>
        <div style="font-size:0.68rem;color:var(--vp-c-text-3);font-family:var(--vp-font-family-mono);text-align:center;">
          <span style="color:#0097a7;">buf[2..5] → KEY "name"</span>
          &nbsp;|&nbsp;
          <span style="color:#9c27b0;">buf[9..13] → STRING "Alice"</span>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">zero-copy pointer stored in TapeNode</div></div>
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">TapeArena</div>
      <div class="bd-group__body">
        <div class="bd-tape-strip" style="justify-content:center;">
          <div class="bd-tape-cell bd-tape-cell--key">
            <span class="bd-tape-cell__idx">tape[1]</span>
            <span class="bd-tape-cell__tag">KEY 0x05</span>
            <span class="bd-tape-cell__val">ptr=&amp;buf[2] len=4</span>
          </div>
          <div class="bd-tape-cell bd-tape-cell--str">
            <span class="bd-tape-cell__idx">tape[2]</span>
            <span class="bd-tape-cell__tag">STRING 0x06</span>
            <span class="bd-tape-cell__val">ptr=&amp;buf[9] len=5</span>
          </div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">.as&lt;string_view&gt;() reads TapeNode → returns pointer into YOUR buffer</div></div>
    <div class="bd-callout bd-callout--green" style="font-size:0.82rem;margin:0;">
      <strong>Zero bytes copied.</strong> The string_view points directly into your original buffer — no allocation, no heap, no copy.
    </div>
  </div>
</div>

**Lifetime rule:** A `string_view` from qbuem-json is valid only while **both** the `Document` and the input buffer are alive. The moment either is destroyed, every `string_view` from that parse becomes a dangling pointer.

### Integer Storage: Fully Inline

```cpp
// How 42 is stored in tape[2] as INT64:
uint64_t raw = (uint64_t(0x08) << 56) | uint64_t(42);
//             ↑ type tag               ↑ value

// How .as<int64_t>() recovers it:
int64_t value = static_cast<int64_t>(raw << 8) >> 8;  // sign-extend 56 bits
// Result: 42  — zero allocations, zero heap access
```

### Jump Indices: O(1) Skip

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand" style="max-width:340px;">{ "a": [1, 2, 3], "b": true }</div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">single-pass parse</div></div>
    <div class="bd-group" style="width:100%;">
      <div class="bd-group__title">TapeArena</div>
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
    <div class="bd-callout bd-callout--green" style="margin:0;font-size:0.82rem;">
      <strong>To skip the array and find "b":</strong> <code>tape[tape[2].jump]</code> = tape[6] = ARR_END → next key at tape[7].
      One array read. <strong>O(1) regardless of array size.</strong>
    </div>
  </div>
</div>

---

## Part 3 — Error Taxonomy

Explore every error category interactively — from what causes it to how to fix it:

<ParseErrorMap />

---

## Part 4 — Debugging by Level

### 🟢 Beginner: Catching and Reading Errors

**All qbuem-json errors derive from `qbuem::json::error`.**

```cpp
#include <qbuem_json/qbuem_json.hpp>

try {
    auto doc  = qbuem::parse(input);
    auto name = doc.root()["name"].as<std::string_view>();
    auto age  = doc.root()["age"].as<int>();
}
catch (const qbuem::parse_error& e) {
    // Malformed JSON — bad input from network, file, etc.
    std::cerr << "Parse failed: " << e.what()   << "\n";
    std::cerr << "At byte:      " << e.position() << "\n";
    std::cerr << "Context:      " << e.context()  << "\n";
    // e.context() returns up to 20 bytes around the error site
}
catch (const qbuem::type_error& e) {
    // Wrong .as<T>() call
    std::cerr << "Type error: " << e.what() << "\n";
}
catch (const qbuem::access_error& e) {
    // Key not found, index out of range
    std::cerr << "Access error: " << e.what() << "\n";
}
catch (const qbuem::json::error& e) {
    // Catch-all for any qbuem-json error
    std::cerr << "JSON error: " << e.what() << "\n";
}
```

**The three questions to ask when an error occurs:**

1. **What byte position?** — `e.position()` tells you exactly where in the input
2. **What is the input around that position?** — `e.context()` shows the surrounding bytes
3. **What type tag did the node have?** — a `type_error` tells you what tag was found vs. what was expected

---

### 🟢 Beginner: Type-Safe Access Pattern

Always check before you extract:

```cpp
auto doc  = qbuem::parse(input);
auto root = doc.root();

// ❌ Fragile — throws if key missing or wrong type
auto name = root["name"].as<std::string_view>();

// ✅ Safe — explicit checks
if (!root.is_object()) {
    return error("expected JSON object");
}

auto name_val = root.find("name");
if (!name_val || !name_val->is_string()) {
    return error("missing or non-string 'name' field");
}
auto name = name_val->as<std::string_view>();

// ✅ Also safe — use the 'or_default' variant
auto count = root["count"].as<int>(/*default=*/ 0);
```

---

### 🟡 Intermediate: Inspecting Error Context

When you get a `parse_error`, the context string is your best diagnostic tool:

```cpp
catch (const qbuem::parse_error& e) {
    auto pos = e.position();      // byte offset into input
    auto ctx = e.context();       // up to 20 bytes around the error

    // Print a visual position marker:
    std::cerr << "Input:   " << std::string(ctx) << "\n";
    std::cerr << "         " << std::string(pos % 20, ' ') << "^\n";
    std::cerr << "Message: " << e.what() << "\n";
}
```

**Reading a `type_error` message:**

<div class="bd-callout" style="font-size:0.82rem;">
  <code>qbuem::type_error: expected INT64/UINT64, got STRING at tape[2]</code><br>
  <div class="bd-row" style="gap:2rem;margin-top:0.5rem;font-size:0.72rem;color:var(--vp-c-text-2);justify-content:flex-start;flex-wrap:wrap;">
    <span>↑ what you called <code>.as&lt;&gt;()</code> with</span>
    <span>↑ what the tape actually contains</span>
  </div>
</div>

Check the TapeNode type tags table above to decode the tag name.

---

### 🟡 Intermediate: Validating Before Parsing

For hot paths receiving untrusted input (network, files), validate cheaply before a full parse:

```cpp
// Quick structural check — does not allocate
bool looks_valid = qbuem::validate(input);

// Check document size (prevent tape exhaustion DoS)
if (input.size() > MAX_DOCUMENT_BYTES) {
    return reject("document too large");
}

// Only parse if validation passes
if (looks_valid) {
    auto root = qbuem::parse(doc, input);
    // ...
}
```

---

### 🟡 Intermediate: Lifetime Safety Patterns

The most common source of hard-to-debug crashes in qbuem-json code is lifetime violations:

```cpp
// ❌ WRONG — string_view outlives the input
std::string_view get_name(std::string_view json_text) {
    qbuem::Document doc;
    auto root = qbuem::parse(doc, json_text);
    return root["name"].as<std::string_view>();
    // ^ This string_view points into json_text.
    //   If json_text was a temporary std::string, it's now dangling.
}

// ✅ CORRECT — return owned data
std::string get_name(std::string_view json_text) {
    qbuem::Document doc;
    auto root = qbuem::parse(doc, json_text);
    auto sv   = root["name"].as<std::string_view>();
    return std::string(sv);  // copy out before return
}

// ✅ CORRECT — keep everything alive together
struct ParseResult {
    std::string      input;     // keeps the buffer alive
    qbuem::Document  doc;       // keeps the tape alive
    qbuem::Value     root;      // safe to use
};

ParseResult parse_and_keep(std::string json) {
    ParseResult r;
    r.input = std::move(json);
    r.root  = qbuem::parse(r.doc, r.input);
    return r;  // everything lives together — safe
}
```

**Rule of thumb:** If you need a `string_view` or `Value` to outlive the local scope, either copy it to `std::string` or move the `Document` and input buffer into the same struct.

---

### 🔴 Expert: Dumping the Tape

When you have a hard-to-reproduce parse bug, dump the tape directly to understand what the parser saw:

```cpp
#include <qbuem_json/qbuem_json.hpp>  // debug utilities

qbuem::Document doc;
auto root = qbuem::parse(doc, input);

// Print the entire tape as human-readable text
qbuem::debug::dump_tape(doc, std::cerr);
```

Sample output:

```
TapeArena — 6 nodes, capacity 256
────────────────────────────────────────────────────────
tape[0]  OBJ_START  tag=0x01  jump→5
tape[1]  KEY        tag=0x05  ptr=0x7ffe1234  len=2  → "id"
tape[2]  INT64      tag=0x08  value=42
tape[3]  KEY        tag=0x05  ptr=0x7ffe1238  len=6  → "active"
tape[4]  BOOL_TRUE  tag=0x0A
tape[5]  OBJ_END    tag=0x02  jump→0
────────────────────────────────────────────────────────
Input buffer: 0x7ffe1230  length: 26
Stage1Index:  24 structural positions
```

**What to look for in the dump:**

| Symptom | Cause |
|:---|:---|
| Node count much lower than expected | Parser stopped early — check for errors |
| KEY node count ≠ STRING node count | Malformed object (missing value for a key) |
| Jump index points to wrong node | Nested depth mismatch — mismatched braces |
| STRING ptr very different from KEY ptr | Input buffer layout issue |
| `DOUBLE` where you expected `INT64` | Number has a decimal point or exponent in JSON |

---

### 🔴 Expert: AddressSanitizer for Lifetime Bugs

qbuem-json's zero-copy design means lifetime bugs produce **no exception** — only silent UB that may or may not crash. AddressSanitizer catches them reliably:

```bash
# Build with ASan
clang++ -fsanitize=address,undefined -g -O1 your_code.cpp -o app

# Run — ASan reports use-after-free on dangling string_view
./app
```

A dangling `string_view` from qbuem-json produces an ASan report like:

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 5 at 0x... thread T0
    #0 std::string_view::operator[] src/your_code.cpp:42
    #1 your_function               src/your_code.cpp:42

0x... is located inside of 26-byte region [0x..., 0x...)
freed by thread T0 here:
    #0 operator delete[]           <system>
    #1 std::string::~string        src/your_code.cpp:38
```

The line numbers in the `freed by` section show exactly where the input buffer was destroyed. Compare it with the `READ of size` location to find the mismatch.

---

### 🔴 Expert: SIMD Edge Cases

A small number of inputs trigger non-obvious Stage 1 behaviour. These are **not bugs** — they are correctness invariants that every advanced user should know:

#### Very long strings (>= 2³² bytes)

The 56-bit payload holds a 48-bit virtual address and an 8-bit length hint. For strings longer than 255 bytes, the 8-bit length overflows into a sentinel value and Stage 2 falls back to a scalar re-scan to compute the true length. Parse correctness is preserved; Stage 1 throughput is unchanged, but Stage 2 has a small additional cost for that specific string token.

#### NEON path on ARM: no `VCOMPRESSB`

ARM NEON lacks the `VCOMPRESSB` instruction. On ARM64, qbuem-json uses a `VBSL`-based gather instead. The structural iteration in Stage 2 uses a slightly longer scalar loop. Throughput on ARM is approximately 40-60% of the AVX-512 path, which is still 20–30× faster than a naive scalar parser.

#### Escaped quotes near the 64-byte boundary

```
// Input crossing a 64-byte window boundary:
// window N ends at:       ...\"
// window N+1 starts at:   value"...
```

The prefix-XOR carry value is passed between windows. If an escaped quote (`\"`) sits at the boundary, the carry must correctly suppress the `"` in window N+1. qbuem-json propagates the carry via a thread-local scalar variable. If you find a parse bug that only appears on inputs whose escaped-quote positions are multiples of 64 bytes, this is the area to investigate.

---

## Part 5 — Quick Diagnostic Checklist

Use this when an error occurs and you're not sure where to start:

### Parse errors

<div class="bd-checklist">
  <div class="bd-checklist-item">Is the JSON syntactically valid? Test with <code>jq . &lt; input.json</code> or an online validator.</div>
  <div class="bd-checklist-item">Does the input have trailing commas? (JSON disallows them; JavaScript allows them.)</div>
  <div class="bd-checklist-item">Are all strings closed with a matching <code>"</code>?</div>
  <div class="bd-checklist-item">Are all <code>{</code> matched with <code>}</code> and <code>[</code> matched with <code>]</code>?</div>
  <div class="bd-checklist-item">Are escape sequences valid? (<code>\"</code>, <code>\\</code>, <code>\/</code>, <code>\b</code>, <code>\f</code>, <code>\n</code>, <code>\r</code>, <code>\t</code>, <code>\uXXXX</code> only)</div>
  <div class="bd-checklist-item">Does any integer exceed 2⁵⁶ − 1? Use double or string instead.</div>
  <div class="bd-checklist-item">Is the input null-terminated, or is the length passed correctly?</div>
</div>

### Type errors

<div class="bd-checklist">
  <div class="bd-checklist-item">What does <code>dump_tape()</code> show as the actual type tag?</div>
  <div class="bd-checklist-item">Did you call <code>.as&lt;int&gt;()</code> on a float like <code>1.0</code>? Use <code>.as&lt;double&gt;()</code> instead.</div>
  <div class="bd-checklist-item">Did you call <code>.as&lt;string_view&gt;()</code> on a key that might not exist? Use <code>.find()</code> first.</div>
  <div class="bd-checklist-item">Are array indices zero-based and within range?</div>
</div>

### Lifetime / crash

<div class="bd-checklist">
  <div class="bd-checklist-item">Is the input buffer still alive when you access <code>string_view</code> values?</div>
  <div class="bd-checklist-item">Is the <code>Document</code> still alive when you use any <code>Value</code> from it?</div>
  <div class="bd-checklist-item">Did you return a <code>Value</code> or <code>string_view</code> from a function where the <code>Document</code> was local?</div>
  <div class="bd-checklist-item">Did you move the <code>Document</code> after parsing (moving invalidates all <code>Value</code> handles)?</div>
  <div class="bd-checklist-item">Run with <code>-fsanitize=address</code> — is there a use-after-free report?</div>
</div>

### Serialization errors

<div class="bd-checklist">
  <div class="bd-checklist-item">Are you passing a fixed-size <code>char[]</code> buffer? Use <code>std::string</code> with <code>reserve()</code> instead.</div>
  <div class="bd-checklist-item">Is the <code>write_to</code> return value checked for overflow errors?</div>
  <div class="bd-checklist-item">Are custom serialization macros (<code>BEAST_SERIALIZE</code>) matching the struct fields exactly?</div>
</div>

---

## Summary

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-pipeline">
      <div class="bd-pipe-stage">
        <span class="bd-pipe-stage__label">Input</span>
        <span class="bd-pipe-stage__main">Your JSON bytes</span>
      </div>
      <div class="bd-pipe-arrow">→</div>
      <div class="bd-pipe-stage bd-box--teal" style="border-color:#0097a7;background:rgba(0,151,167,0.08);">
        <span class="bd-pipe-stage__label">Stage 1</span>
        <span class="bd-pipe-stage__main">SIMD</span>
      </div>
      <div class="bd-pipe-arrow">→</div>
      <div class="bd-pipe-stage">
        <span class="bd-pipe-stage__label">Stage 2</span>
        <span class="bd-pipe-stage__main">Scalar</span>
      </div>
      <div class="bd-pipe-arrow">→</div>
      <div class="bd-pipe-stage bd-box--brand" style="border-color:var(--vp-c-brand-1);background:color-mix(in srgb,var(--vp-c-brand-1) 8%,transparent);">
        <span class="bd-pipe-stage__label">Result</span>
        <span class="bd-pipe-stage__main">TapeArena</span>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-box bd-box--brand"><code>qbuem::Value</code> <small>— lazy handle (16 bytes)</small></div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">.as&lt;T&gt;() on demand</div></div>
    <div class="bd-box bd-box--green">Typed C++ value <small>— zero allocation</small></div>
    <div class="bd-row" style="gap:1rem;margin-top:0.75rem;flex-wrap:wrap;">
      <div class="bd-callout bd-callout--orange" style="flex:1;min-width:200px;margin:0;font-size:0.78rem;">
        <strong>Input buffer</strong> must stay alive for the entire pipeline above.
      </div>
      <div class="bd-callout" style="flex:1;min-width:200px;margin:0;font-size:0.78rem;">
        <strong>TapeArena</strong> is owned by Document — lives as long as it does.
      </div>
      <div class="bd-callout bd-callout--red" style="flex:1;min-width:200px;margin:0;font-size:0.78rem;">
        <strong>Values</strong> are 16-byte handles — safe to copy, dangerous to outlive their Document.
      </div>
    </div>
  </div>
</div>

If in doubt: **copy `string_view` to `std::string`** before the input or `Document` goes out of scope. Every other performance characteristic of qbuem-json survives this one safety net.
