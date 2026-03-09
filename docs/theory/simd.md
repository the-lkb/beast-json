# SIMD Acceleration: Bitsliced Structural Analysis

Beast JSON replaces a character-by-character state machine with a **data-parallel byte classification engine**. Rather than branching on each byte, it classifies 64 bytes simultaneously using a single AVX-512 register, producing a sparse bitset of structural positions in a fraction of the time.

---

## The Scalar Baseline — Why It's Slow

A naive JSON scanner must branch on every byte:

```cpp
for (size_t i = 0; i < len; ++i) {
    char c = input[i];
    if      (c == '{') tape_push(OBJ_START);
    else if (c == '}') tape_push(OBJ_END);
    else if (c == '"') handle_string(i);
    // ... 6 more branches
}
```

On a modern superscalar CPU, this produces:
- A branch per byte → branch predictor thrash on real-world JSON
- One byte processed per iteration → unable to exploit instruction-level parallelism
- Maximum throughput: ~1 byte/cycle → ~3 GB/s at 3 GHz

Beast JSON's SIMD path achieves **64 bytes per cycle** on AVX-512 — a 64× improvement in classification throughput.

---

## Stage 1 → Stage 2: Interactive Pipeline Walkthrough

Step through the SIMD pipeline interactively — from raw bytes to TapeNodes:

<SimdPipeline />

On Intel Ice Lake and later, `VMOVDQU64` has 1 cycle latency and can be pipelined — the CPU overlaps loading window N+1 while processing window N.

---

## Stage 1a: Parallel Structural Character Detection

Instead of eight `if` branches, Beast JSON runs eight `VPCMPEQB` instructions. Each compares all 64 bytes against one target character and produces a 64-bit bitmask. For a 64-byte window, this produces a **64-bit integer** (`structural_mask`) identifying every structural character in **~8 cycles total**.

### What the mask looks like

For the input `{ "name": "Alice" }` (first 20 bytes shown):

<div class="bd-mask-table">
  <div class="bd-mt-row">
    <span class="bd-mt-label">Byte</span>
    <span class="bd-mt-cell bd-mt-cell--idx">0</span><span class="bd-mt-cell bd-mt-cell--idx">1</span><span class="bd-mt-cell bd-mt-cell--idx">2</span><span class="bd-mt-cell bd-mt-cell--idx">3</span><span class="bd-mt-cell bd-mt-cell--idx">4</span><span class="bd-mt-cell bd-mt-cell--idx">5</span><span class="bd-mt-cell bd-mt-cell--idx">6</span><span class="bd-mt-cell bd-mt-cell--idx">7</span><span class="bd-mt-cell bd-mt-cell--idx">8</span><span class="bd-mt-cell bd-mt-cell--idx">9</span><span class="bd-mt-cell bd-mt-cell--idx">10</span><span class="bd-mt-cell bd-mt-cell--idx">11</span><span class="bd-mt-cell bd-mt-cell--idx">12</span><span class="bd-mt-cell bd-mt-cell--idx">13</span><span class="bd-mt-cell bd-mt-cell--idx">14</span><span class="bd-mt-cell bd-mt-cell--idx">15</span><span class="bd-mt-cell bd-mt-cell--idx">16</span><span class="bd-mt-cell bd-mt-cell--idx">17</span><span class="bd-mt-cell bd-mt-cell--idx">18</span><span class="bd-mt-cell bd-mt-cell--idx">19</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">Input</span>
    <span class="bd-mt-cell bd-mt-cell--struct">{</span><span class="bd-mt-cell"> </span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell">n</span><span class="bd-mt-cell">a</span><span class="bd-mt-cell">m</span><span class="bd-mt-cell">e</span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell bd-mt-cell--struct">:</span><span class="bd-mt-cell"> </span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell">A</span><span class="bd-mt-cell">l</span><span class="bd-mt-cell">i</span><span class="bd-mt-cell">c</span><span class="bd-mt-cell">e</span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell"> </span><span class="bd-mt-cell"> </span><span class="bd-mt-cell bd-mt-cell--struct">}</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">Mask</span>
    <span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span>
  </div>
  <div class="bd-mt-annotation">
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,var(--vp-c-brand-1) 15%,transparent);color:var(--vp-c-brand-1);"><strong>■</strong> structural char detected</span>
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,#4caf50 15%,transparent);color:#4caf50;"><strong>1</strong> = set in mask</span>
    <span class="bd-mt-ann-chip" style="color:var(--vp-c-text-3);">0 = cleared</span>
  </div>
</div>

---

## Stage 1b: Quote-Region Masking (Prefix-XOR Carry)

The raw `structural_mask` still includes characters **inside string literals** — e.g., a `:` inside `"key:val"`. Beast JSON uses a **prefix-XOR carry** to suppress them.

The core insight: `in_string[i] = XOR of all unescaped quote bits from index 0 to i`.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="max-width:520px;width:100%;">
      <div class="bd-group__title">Step 1 — Locate backslashes and quotes</div>
      <div class="bd-group__body bd-group__body--row">
        <div class="bd-box bd-box--orange">backslash_mask<br><small>bit=1 at each '\' position</small></div>
        <div class="bd-box bd-box--purple">raw_quote_mask<br><small>bit=1 at each '"' position</small></div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="max-width:520px;width:100%;">
      <div class="bd-group__title">Step 2 — Suppress escaped quotes</div>
      <div class="bd-group__body">
        <div class="bd-box">escape_mask = backslash_mask <strong>shift left 1</strong><br><small>(marks the byte AFTER each backslash)</small></div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--green">real_quote_mask = raw_quote_mask <strong>AND NOT</strong> escape_mask<br><small>(removes escaped quotes like \")</small></div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="max-width:520px;width:100%;">
      <div class="bd-group__title">Step 3 — Prefix-XOR via CLMUL (4 cycles)</div>
      <div class="bd-group__body">
        <div class="bd-box bd-box--teal">PCLMULQDQ real_quote_mask, 0xFFFF…<br><small>carryless multiply = prefix XOR</small></div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--brand">in_string_mask<br><small>bit[i] = XOR(real_quote_mask[0..i]) — 0=outside string, 1=inside</small></div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-box bd-box--green" style="max-width:440px;">
      clean_structural_mask = structural_mask <strong>AND NOT</strong> in_string_mask
    </div>
  </div>
</div>

### Worked example: colon inside a string

Input: `{"key:val":1}` — the `:` at byte 5 is inside a string and must be suppressed.

<div class="bd-mask-table">
  <div class="bd-mt-row">
    <span class="bd-mt-label">Byte</span>
    <span class="bd-mt-cell bd-mt-cell--idx">0</span><span class="bd-mt-cell bd-mt-cell--idx">1</span><span class="bd-mt-cell bd-mt-cell--idx">2</span><span class="bd-mt-cell bd-mt-cell--idx">3</span><span class="bd-mt-cell bd-mt-cell--idx">4</span><span class="bd-mt-cell bd-mt-cell--idx">5</span><span class="bd-mt-cell bd-mt-cell--idx">6</span><span class="bd-mt-cell bd-mt-cell--idx">7</span><span class="bd-mt-cell bd-mt-cell--idx">8</span><span class="bd-mt-cell bd-mt-cell--idx">9</span><span class="bd-mt-cell bd-mt-cell--idx">10</span><span class="bd-mt-cell bd-mt-cell--idx">11</span><span class="bd-mt-cell bd-mt-cell--idx">12</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">Input</span>
    <span class="bd-mt-cell bd-mt-cell--struct">{</span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell bd-mt-cell--in-str">k</span><span class="bd-mt-cell bd-mt-cell--in-str">e</span><span class="bd-mt-cell bd-mt-cell--in-str">y</span><span class="bd-mt-cell bd-mt-cell--false-pos">:</span><span class="bd-mt-cell bd-mt-cell--in-str">v</span><span class="bd-mt-cell bd-mt-cell--in-str">a</span><span class="bd-mt-cell bd-mt-cell--in-str">l</span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell bd-mt-cell--struct">:</span><span class="bd-mt-cell">1</span><span class="bd-mt-cell bd-mt-cell--struct">}</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">quote_mask</span>
    <span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">in_string</span>
    <span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span>
  </div>
  <div class="bd-mt-row bd-mt-row--spacer"></div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">raw_struct</span>
    <span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--false-pos">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">clean_struct</span>
    <span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--suppressed">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span>
  </div>
  <div class="bd-mt-annotation">
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,#e91e63 12%,transparent);color:#e91e63;"><strong>■</strong> inside string</span>
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,#ff5722 22%,transparent);color:#ff5722;"><strong>■</strong> false positive ← suppressed</span>
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,#4caf50 15%,transparent);color:#4caf50;"><strong>1</strong> real structural char</span>
  </div>
</div>

Byte 5 (`:` inside the string): `raw_struct=1` → `in_string=1` → `clean_struct=0`. Correctly suppressed in one bitwise AND NOT operation.

---

## Stage 1c: Structural Byte Extraction (VCOMPRESSB)

On Intel Ice Lake+, `VCOMPRESSB` packs the flagged bytes into a dense output in **one instruction**:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-split" style="max-width:540px;width:100%;gap:1rem;">
      <div class="bd-col">
        <div class="bd-group__title" style="font-size:0.7rem;text-transform:uppercase;color:var(--vp-c-text-2);letter-spacing:.07em;margin-bottom:.35rem;">Before — zmm0 (64 bytes, sparse)</div>
        <div class="bd-box" style="font-size:0.75rem;font-family:monospace;letter-spacing:0.05em;">{ · · · · · · · · · · · · · ·<br>· · · · · · · · · } · · · · ·</div>
        <div class="bd-badge bd-badge--brand" style="margin-top:.35rem;">64 bytes total</div>
      </div>
      <div class="bd-col">
        <div class="bd-group__title" style="font-size:0.7rem;text-transform:uppercase;color:var(--vp-c-text-2);letter-spacing:.07em;margin-bottom:.35rem;">After — zmm1 (dense structural bytes)</div>
        <div class="bd-box bd-box--green" style="font-size:0.85rem;font-family:monospace;">{ }</div>
        <div class="bd-badge bd-badge--green" style="margin-top:.35rem;">2 bytes — structural only</div>
      </div>
    </div>
    <div class="bd-callout" style="max-width:540px;width:100%;margin:0.75rem 0 0;font-size:0.8rem;">
      <strong>VCOMPRESSB zmm1 {k1}, zmm0</strong> — one instruction, ~3 cycles<br>
      k1 = clean_structural_mask; set bits select which bytes to keep
    </div>
  </div>
</div>

Stage 2 now iterates a **tiny dense buffer** — only structural characters — rather than the full input.

---

## Stage 2: Tape Generation via Bitset Iteration

Stage 2 uses `TZCNT` (trailing zero count) to iterate only the set bits in `clean_structural_mask`:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand" style="max-width:400px;font-size:0.78rem;">
      clean_structural_mask<br><small style="color:var(--vp-c-text-2);">e.g. 0b…0001001010010001</small>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-split" style="width:100%;max-width:600px;gap:1rem;">
      <div class="bd-group">
        <div class="bd-group__title">Per-structural-character loop</div>
        <div class="bd-group__body">
          <div class="bd-steps">
            <div class="bd-step"><div class="bd-step__num">1</div><div class="bd-step__body"><div class="bd-step__title">TZCNT</div><div class="bd-step__desc">Find index of next structural char (count trailing zeros — 1 cycle)</div></div></div>
            <div class="bd-step"><div class="bd-step__num">2</div><div class="bd-step__body"><div class="bd-step__title">Load input[index]</div><div class="bd-step__desc">Read the structural character</div></div></div>
            <div class="bd-step"><div class="bd-step__num">3</div><div class="bd-step__body"><div class="bd-step__title">switch(char)</div><div class="bd-step__desc">8-way dispatch — emit TapeNode</div></div></div>
            <div class="bd-step"><div class="bd-step__num">4</div><div class="bd-step__body"><div class="bd-step__title">BLSR</div><div class="bd-step__desc">Clear lowest set bit → advance to next (1 cycle) → back to step 1</div></div></div>
          </div>
        </div>
      </div>
      <div class="bd-group">
        <div class="bd-group__title">TapeNode emitted per case</div>
        <div class="bd-group__body">
          <div class="bd-box bd-box--teal" style="font-size:0.75rem;">{ } [ ]<br><small>→ OBJ/ARR node + jump-patch</small></div>
          <div class="bd-box bd-box--purple" style="font-size:0.75rem;">"<br><small>→ KEY or STRING string_view</small></div>
          <div class="bd-box bd-box--green" style="font-size:0.75rem;">digit / -<br><small>→ UINT64 / INT64 / DOUBLE</small></div>
          <div class="bd-box bd-box--orange" style="font-size:0.75rem;">t / f / n<br><small>→ BOOL_TRUE / FALSE / NULL</small></div>
        </div>
      </div>
    </div>
  </div>
</div>

The loop body executes **once per structural character**. In typical JSON, structural characters are 5–15% of the input — Stage 2 is extremely cache-efficient.

---

## ARM NEON Path

On Apple Silicon and ARM64 servers, Beast JSON uses NEON 128-bit registers (16 bytes per load). The algorithm is identical; 4 NEON iterations cover 64 bytes:

<div class="bd-diagram">
  <div class="bd-group" style="max-width:480px;margin:0 auto;">
    <div class="bd-group__title">One NEON iteration — 16 bytes</div>
    <div class="bd-group__body">
      <div class="bd-pipeline">
        <div class="bd-pipe-stage">
          <div class="bd-pipe-stage__label">Load</div>
          <div class="bd-pipe-stage__main">VLD1Q_U8 q0, [buf]</div>
          <div class="bd-pipe-stage__note">16 bytes · 1 cycle</div>
        </div>
        <div class="bd-pipe-arrow">→</div>
        <div class="bd-pipe-stage">
          <div class="bd-pipe-stage__label">Compare</div>
          <div class="bd-pipe-stage__main">VCEQQ_U8 ×8</div>
          <div class="bd-pipe-stage__note">vs each structural target</div>
        </div>
        <div class="bd-pipe-arrow">→</div>
        <div class="bd-pipe-stage">
          <div class="bd-pipe-stage__label">Merge</div>
          <div class="bd-pipe-stage__main">VORRQ_U8</div>
          <div class="bd-pipe-stage__note">all 8 masks → one</div>
        </div>
      </div>
    </div>
  </div>
</div>

NEON has no `VCOMPRESSB` equivalent. Beast JSON uses a `VBSL`-based gather with a compact scalar loop for Stage 2 on ARM — still far faster than a pure scalar parser.

---

## Throughput Summary

| Architecture | Register width | Bytes/cycle (Stage 1) | Throughput @ 3 GHz |
|:---|:---|:---|:---|
| x86-64 AVX-512 | 512-bit ZMM | ~8 bytes/cycle | ~24 GB/s (register bandwidth) |
| ARM NEON | 128-bit Q | ~4 bytes/cycle | ~12 GB/s |
| Scalar reference | 8-bit GPR | ~1 byte/cycle | ~3 GB/s |

End-to-end parse throughput (2.7 GB/s) is below the Stage-1 ceiling because memory bandwidth and Stage-2 tape generation are the bottleneck for large documents.

---

## Instruction Reference

| Instruction | ISA | Operation | Latency |
|:---|:---|:---|---:|
| `VMOVDQU64` | AVX-512 | Load 64 bytes unaligned | 1 cycle |
| `VPCMPEQB` | AVX-512 | Compare 64 bytes → 64-bit mask | 1 cycle |
| `KORQ` | AVX-512 | OR two 64-bit k-registers | 1 cycle |
| `PCLMULQDQ` | PCLMULQDQ | Carryless multiply (prefix XOR) | 4 cycles |
| `VCOMPRESSB` | AVX-512 VBMI | Pack masked bytes to dense | 3 cycles |
| `TZCNT` | BMI1 | Count trailing zeros | 3 cycles |
| `BLSR` | BMI1 | Reset lowest set bit | 1 cycle |
| `VLD1Q_U8` | NEON | Load 16 bytes | 1 cycle |
| `VCEQQ_U8` | NEON | Compare 16 bytes | 1 cycle |
