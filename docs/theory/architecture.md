# The DOM Tape DOM

A 50 KB JSON document with 1,000 string values makes `nlohmann/json` call `malloc` over 1,000 times. Each node ends up at a random heap address. When you traverse the result, every key and every value is a pointer chase to a different cache line. The CPU's prefetcher stops trying.

qbuem-json parses the same document with **one allocation**. Here's why that's possible and how it works.

---

## The problem isn't parsing — it's what parsers build

Tree-based parsers build a tree. That sounds obvious, but it has a specific performance implication: a tree requires one heap node per element, and those nodes land wherever `malloc` puts them. For this JSON:

```json
{ "id": 101, "name": "Alice", "active": true }
```

`nlohmann/json` allocates something like this:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:540px;">
      <div class="bd-group__title">nlohmann/json — 3 fields, 6+ heap allocations, scattered addresses</div>
      <div class="bd-group__body" style="font-family:var(--vp-font-family-mono);font-size:0.78rem;line-height:1.9;">
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:0.3rem 1.5rem;padding:0.25rem 0;">
          <div class="bd-box bd-box--orange" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x5a40 Object node<br><small style="color:var(--vp-c-text-3);">→ ptr → children vector</small></div>
          <div class="bd-box bd-box--orange" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x7f23 children vec<br><small style="color:var(--vp-c-text-3);">heap-allocated</small></div>
          <div class="bd-box" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x3c11 key "id"<br><small style="color:var(--vp-c-text-3);">heap-copied string</small></div>
          <div class="bd-box" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x12ef IntNode(101)<br><small style="color:var(--vp-c-text-3);">separate heap object</small></div>
          <div class="bd-box" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x9d04 key "name"<br><small style="color:var(--vp-c-text-3);">heap-copied string</small></div>
          <div class="bd-box" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x4a17 StringNode "Alice"<br><small style="color:var(--vp-c-text-3);">heap-copied string</small></div>
        </div>
        <div style="margin-top:0.5rem;font-size:0.75rem;color:var(--vp-c-text-2);">
          3 fields → 6 cache lines touched on every traversal
        </div>
      </div>
    </div>
  </div>
</div>

Two things make this slow, and they're related. First, every access chases a pointer to an unpredictable address — the CPU can't prefetch what it can't predict. Second, every string gets copied at parse time, whether you ever read it or not. A 100-field JSON payload where you only need 3 fields still pays for all 100 copies.

There's also a third problem: when you want to skip a nested structure — say, a 500-element array under a key you don't care about — a tree-based DOM has no choice but to walk all 500 elements.

---

## A flat array fixes all three

qbuem-json doesn't build a tree. It writes a **flat contiguous array** — the tape — with one 8-byte slot per JSON element. Same input, entirely different layout:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand" style="max-width:360px;font-family:monospace;">{ "id": 101, "name": "Alice", "active": true }</div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">single-pass parse — one malloc</div></div>
    <div class="bd-tape-strip">
      <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[0]</span><span class="bd-tape-cell__tag">OBJ_START</span><span class="bd-tape-cell__val">jump→7</span></div>
      <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[1]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">&amp;buf[2] "id"</span></div>
      <div class="bd-tape-cell bd-tape-cell--int"><span class="bd-tape-cell__idx">tape[2]</span><span class="bd-tape-cell__tag">INT64</span><span class="bd-tape-cell__val">101</span></div>
      <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[3]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">&amp;buf[8] "name"</span></div>
      <div class="bd-tape-cell bd-tape-cell--str"><span class="bd-tape-cell__idx">tape[4]</span><span class="bd-tape-cell__tag">STRING</span><span class="bd-tape-cell__val">&amp;buf[16] "Alice"</span></div>
      <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[5]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">&amp;buf[26] "active"</span></div>
      <div class="bd-tape-cell bd-tape-cell--bool"><span class="bd-tape-cell__idx">tape[6]</span><span class="bd-tape-cell__tag">BOOL_TRUE</span><span class="bd-tape-cell__val">—</span></div>
      <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[7]</span><span class="bd-tape-cell__tag">OBJ_END</span><span class="bd-tape-cell__val">jump→0</span></div>
    </div>
    <div class="bd-callout" style="font-size:0.8rem;margin-top:0.5rem;">
      8 nodes × 8 bytes = 64 bytes. <strong>Exactly one CPU cache line.</strong>
    </div>
  </div>
</div>

Notice what's different. The nodes are sequential — the CPU prefetcher can get all of them in one cache line fill. The strings are not copied — KEY and STRING nodes store a pointer into the original input buffer. And the `OBJ_START` node already knows where the object ends: that `jump→7` is how you skip an entire subtree in one array read.

The flat array is the whole design. Every other mechanism is a consequence of it.

---

## But first, qbuem-json doesn't read most of your input

Before writing a single tape node, qbuem-json runs a SIMD scan that classifies **64 bytes at once** using a single AVX-512 instruction. The output is a 64-bit bitmask — one bit per byte — where a `1` marks a structural character (`{`, `}`, `[`, `]`, `"`, `:`, `,`) and `0` marks data.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:580px;">
      <div class="bd-group__title">Stage 1 — one AVX-512 pass, 64 bytes in, 64-bit mask out</div>
      <div class="bd-group__body">
        <div class="bd-pipeline">
          <div class="bd-pipe-stage">
            <div class="bd-pipe-stage__label">Load</div>
            <div class="bd-pipe-stage__main">VMOVDQU64</div>
            <div class="bd-pipe-stage__note">64 bytes → ZMM0</div>
          </div>
          <div class="bd-pipe-arrow">→</div>
          <div class="bd-pipe-stage">
            <div class="bd-pipe-stage__label">Classify</div>
            <div class="bd-pipe-stage__main">VPCMPEQB ×7</div>
            <div class="bd-pipe-stage__note">one mask per structural char type</div>
          </div>
          <div class="bd-pipe-arrow">→</div>
          <div class="bd-pipe-stage">
            <div class="bd-pipe-stage__label">Merge + Filter</div>
            <div class="bd-pipe-stage__main">KORQ + PCLMULQDQ</div>
            <div class="bd-pipe-stage__note">OR masks, suppress chars inside strings</div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

The tape-writing pass (Stage 2) then iterates only the `1` bits using `TZCNT` — one instruction that finds the next set bit in a single cycle. Typical JSON has 5–15% structural characters. Stage 2 touches 5–15% of the input. The rest of your bytes are never visited.

On ARM (Apple Silicon, Linux aarch64), four NEON 16-byte loads replace one AVX-512 load. The bitmask algorithm is identical.

> Full SIMD mechanics, including the prefix-XOR string-suppression step: [SIMD Acceleration →](/theory/simd)

---

## Eight bytes is enough for anything

Every JSON type — object, array, string, integer, float, boolean, null — fits in the same 8-byte layout:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-bits">
      <div class="bd-bit-seg" style="width:90px;flex-shrink:0;background:color-mix(in srgb,var(--vp-c-brand-1) 22%,transparent);border-radius:4px 0 0 4px;">
        <span class="bd-bit-seg__range">bits 63–56</span>
        <span class="bd-bit-seg__val">Type Tag</span>
        <span class="bd-bit-seg__name">8 bits — 12 types</span>
      </div>
      <div class="bd-bit-seg" style="flex:1;background:color-mix(in srgb,var(--vp-c-brand-1) 9%,transparent);border:1px solid var(--vp-c-divider);border-radius:0 4px 4px 0;">
        <span class="bd-bit-seg__range">bits 55–0</span>
        <span class="bd-bit-seg__val">Payload</span>
        <span class="bd-bit-seg__name">56 bits — meaning depends on type</span>
      </div>
    </div>
    <div class="bd-row" style="gap:0.75rem;margin-top:0.75rem;flex-wrap:wrap;">
      <div class="bd-group" style="flex:1;min-width:150px;margin:0;">
        <div class="bd-group__title" style="font-size:0.68rem;">Containers</div>
        <div class="bd-group__body" style="padding:0.35rem;">
          <div class="bd-box bd-box--teal" style="font-size:0.72rem;padding:0.3rem 0.5rem;"><code>OBJ/ARR _START/_END</code><br><small>payload = jump index →<br>other end of the pair</small></div>
        </div>
      </div>
      <div class="bd-group" style="flex:1;min-width:150px;margin:0;">
        <div class="bd-group__title" style="font-size:0.68rem;">Strings</div>
        <div class="bd-group__body" style="padding:0.35rem;">
          <div class="bd-box bd-box--purple" style="font-size:0.72rem;padding:0.3rem 0.5rem;"><code>KEY / STRING</code><br><small>payload = 48-bit ptr<br>into input buffer<br>+ 8-bit length hint</small></div>
        </div>
      </div>
      <div class="bd-group" style="flex:1;min-width:150px;margin:0;">
        <div class="bd-group__title" style="font-size:0.68rem;">Scalars</div>
        <div class="bd-group__body" style="padding:0.35rem;">
          <div class="bd-box bd-box--green" style="font-size:0.72rem;padding:0.3rem 0.5rem;"><code>UINT64 / INT64</code><br><small>value stored inline</small><br><code>DOUBLE</code><br><small>bit-cast from double</small><br><code>BOOL / NULL</code><br><small>type tag only</small></div>
        </div>
      </div>
    </div>
  </div>
</div>

The 56-bit payload is enough to hold a 48-bit virtual address (the actual width on both x86-64 and ARM64) plus an 8-bit string length hint. That means `KEY` and `STRING` nodes carry a full `string_view` in their payload — no heap involved.

Eight bytes per node means **8 nodes per cache line**. An object scan is a sequential memory read that the prefetcher handles automatically.

---

## Strings are never copied

When Stage 2 hits a string or key, it writes the pointer to where the string lives in your input buffer — not the string itself. "Alice" stays at `buf[16]`. The tape stores `{ptr=&buf[16], len=5}`.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:480px;">
      <div class="bd-group__title">Input buffer — caller-owned, never touched by the parser</div>
      <div class="bd-group__body">
        <div class="bd-box bd-box--blue" style="font-family:monospace;">&nbsp;{ "name": "Alice" }<br>
          <span style="font-size:0.7rem;color:var(--vp-c-text-3);">&nbsp;0&nbsp;&nbsp;2&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;9</span>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">Stage 2 writes a pointer, not the data</div></div>
    <div class="bd-tape-strip" style="max-width:480px;">
      <div class="bd-tape-cell bd-tape-cell--key" style="min-width:160px;"><span class="bd-tape-cell__idx">KEY</span><span class="bd-tape-cell__val">ptr=&amp;buf[2] len=4</span></div>
      <div class="bd-tape-cell bd-tape-cell--str" style="min-width:160px;"><span class="bd-tape-cell__idx">STRING</span><span class="bd-tape-cell__val">ptr=&amp;buf[9] len=5</span></div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">.as&lt;string_view&gt;() reads one tape cell → returns view into original buf</div></div>
    <div class="bd-callout bd-callout--green" style="max-width:480px;font-size:0.82rem;">
      Zero bytes copied — at parse time, at navigation time, at extraction time.<br>
      "Alice" was always at buf[9]. The tape just remembers where.
    </div>
  </div>
</div>

The lifetime contract is simple: `string_view` stays valid as long as both the `Document` and the input buffer are alive. If you need the string to outlive the input, `.as<std::string>()` does exactly one copy, on demand, never before.

---

## Skipping a subtree costs one array read

Every `OBJ_START` node stores the tape index of its matching `OBJ_END` in its payload. Same for arrays. This is the jump index.

When you access `root["status"]` on an object that has a 500-field `"metadata"` block before `"status"`, qbuem-json reads `tape[1]` (KEY "metadata"), sees it's not a match, reads `tape[2]` (OBJ_START), then jumps directly to `tape[tape[2].jump + 1]` — skipping all 500 fields in a single integer read.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;">
      <div class="bd-group__title">Tape for: <code>{ "meta": { ...500 fields... }, "status": "ok" }</code></div>
      <div class="bd-group__body">
        <div class="bd-tape-strip">
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[0]</span><span class="bd-tape-cell__tag">OBJ_START</span><span class="bd-tape-cell__val">jump→504</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[1]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">"meta"</span></div>
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[2]</span><span class="bd-tape-cell__tag">OBJ_START</span><span class="bd-tape-cell__val">jump→503</span></div>
          <div class="bd-tape-cell" style="min-width:80px;opacity:0.35;"><span class="bd-tape-cell__idx">tape[3…502]</span><span class="bd-tape-cell__val">500 fields</span></div>
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[503]</span><span class="bd-tape-cell__tag">OBJ_END</span><span class="bd-tape-cell__val">jump→2</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[504]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">"status"</span></div>
          <div class="bd-tape-cell bd-tape-cell--str"><span class="bd-tape-cell__idx">tape[505]</span><span class="bd-tape-cell__tag">STRING</span><span class="bd-tape-cell__val">"ok"</span></div>
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[506]</span><span class="bd-tape-cell__tag">OBJ_END</span><span class="bd-tape-cell__val">jump→0</span></div>
        </div>
        <div class="bd-callout bd-callout--green" style="margin-top:0.5rem;font-size:0.8rem;">
          Looking up "status": KEY "meta" → no match → <code>tape[2].jump = 503</code> → jump to tape[504] KEY "status".<br>
          <strong>500 fields skipped. Cost: two array reads.</strong>
        </div>
      </div>
    </div>
  </div>
</div>

This is the mechanism behind qbuem-json's "100-level deep, 100-wide" benchmark numbers. Every depth level is one jump read.

---

## A `Value` is a position, not a value

After parsing, you get back a `qbuem::Value` — a 16-byte token:

```cpp
struct Value {
    DocumentView* doc_;  // which document
    uint32_t      idx_;  // which tape slot
};
```

Navigation — `root["user"]["profile"]["city"]` — just updates `idx_`. No allocation, no string comparison beyond what's needed to match keys, no heap access. You can navigate to any depth and the cost is proportional only to the number of keys matched, not the size of the document.

Extraction is the one operation that touches data:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:560px;">
      <div class="bd-group__title">What you pay and when</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step">
            <div class="bd-step__num" style="background:color-mix(in srgb,#e53935 18%,transparent);color:#c62828;font-size:0.75rem;">once</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Parse — <code>qbuem::parse(doc, text)</code></div>
              <div class="bd-step__desc">SIMD scan + tape write. Proportional to input size. Never paid again.</div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num" style="background:color-mix(in srgb,#43a047 18%,transparent);color:#2e7d32;font-size:0.75rem;">free</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Navigate — <code>root["user"]["name"]</code></div>
              <div class="bd-step__desc">Tape index arithmetic. No allocation. No extraction.</div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num" style="background:color-mix(in srgb,#1e88e5 18%,transparent);color:#1565c0;font-size:0.75rem;">ask</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Extract — <code>.as&lt;T&gt;()</code></div>
              <div class="bd-step__desc">
                One tape read. For <code>string_view</code>: zero copy.
                For <code>std::string</code>: one copy, right here.
                For integers and doubles: already in the payload, no parsing needed.
                For fields you never call <code>.as()</code> on: cost is zero.
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

<DOMLifecycle />

---

## The second parse is free

The tape lives in a `DocumentView`. When you call `qbuem::parse()` again into the same `Document`, the existing `TapeArena` is reset (cursor back to zero, capacity kept) and reused — no `malloc` as long as the new document fits. The SIMD structural index is also reused the same way.

<TapeFlowDiagram />

In a JSON stream processing loop, the first document pays the allocation cost. Every document after that pays nothing.

```cpp
qbuem::Document doc;          // allocates tape once
while (auto line = read_line()) {
    auto root = qbuem::parse_reuse(doc, line); // zero malloc
    process(root["event"].as<std::string_view>());
}
```

---

## How it compares

<TreeVsTape />

| | qbuem-json DOM | nlohmann/json | simdjson |
|:---|:---|:---|:---|
| Allocations per parse | **1** | O(N elements) | 2 |
| Memory layout | Contiguous tape | Scattered heap | Tape (read-only) |
| String storage | Zero-copy `string_view` | Heap `std::string` | Zero-copy `string_view` |
| Object skip | **O(1)** jump index | O(N) walk | O(1) jump index |
| Mutation support | ✅ overlay map | ✅ in-place | ❌ |
| Serialize support | ✅ | ✅ | ❌ |
| Peak RSS (twitter.json) | **3.4 MB** | 27.4 MB | 11.0 MB |

The gap vs simdjson is mostly mutation and serialization — simdjson's tape is a read-only view and it intentionally doesn't support writing. qbuem-json's tape is writable via an overlay map that stores mutations without touching the original tape.

---

When even a single tape allocation is too much — high-frequency trading, hot API handlers, real-time event streams — there's a path that skips the tape entirely:

**[Nexus Fusion: Zero-Tape →](/theory/nexus-fusion)**
JSON maps directly to your C++ struct fields in one pass. No tape. No intermediate state. No allocation at all.
