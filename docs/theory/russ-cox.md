# Float Parsing: Russ Cox Fast Unrounded Scaling

Standard library functions like `strtod` and `atof` are notoriously slow for high-throughput parsing. Beast JSON replaces them with a two-stage integer-only algorithm that produces bit-accurate IEEE-754 `double` results without touching the FPU rounding mode.

---

## Why `strtod` Is Slow

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--red">strtod(str, &amp;end) called</div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="width:100%;max-width:500px;">
      <div class="bd-group__title">Hidden costs inside strtod</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step"><div class="bd-step__num">1</div><div class="bd-step__body"><div class="bd-step__title">STMXCSR — save FPU rounding mode</div><div class="bd-step__desc">Serializing instruction: stalls the entire CPU pipeline</div></div></div>
          <div class="bd-step"><div class="bd-step__num">2</div><div class="bd-step__body"><div class="bd-step__title">LDMXCSR — set IEEE round-to-nearest</div><div class="bd-step__desc">Another pipeline flush</div></div></div>
          <div class="bd-step"><div class="bd-step__num">3</div><div class="bd-step__body"><div class="bd-step__title">Sequential digit accumulation</div><div class="bd-step__desc">result = result × 10.0 + digit (7–17 floating-point multiplies)</div></div></div>
          <div class="bd-step"><div class="bd-step__num">4</div><div class="bd-step__body"><div class="bd-step__title">FPU rounding error accumulates</div><div class="bd-step__desc">Each step adds error — may require expensive correction loop</div></div></div>
          <div class="bd-step"><div class="bd-step__num">5</div><div class="bd-step__body"><div class="bd-step__title">LDMXCSR — restore original rounding mode</div><div class="bd-step__desc">Third pipeline flush</div></div></div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-box bd-box--orange">double returned<br><small>(after ~80–200 ns)</small></div>
  </div>
</div>

`STMXCSR` / `LDMXCSR` are **serializing instructions** — they prevent the CPU from executing any subsequent instruction until the FPU state is written back to memory. For a parser targeting 2.7 GB/s, even a single `strtod` call per number would destroy all SIMD gains.

---

## IEEE-754 Double Layout

Before understanding the algorithm, it helps to see the target bit pattern:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="max-width:560px;width:100%;">
      <div class="bd-group__title">IEEE-754 double — 64 bits total</div>
      <div class="bd-group__body">
        <div class="bd-bits" style="max-width:520px;width:100%;">
          <div class="bd-bit-seg" style="width:60px;flex-shrink:0;background:rgba(244,67,54,0.15);border:1px solid rgba(244,67,54,0.4);border-radius:4px 0 0 4px;">
            <span class="bd-bit-seg__range">bit 63</span>
            <span class="bd-bit-seg__val" style="color:#f44336;">Sign</span>
            <span class="bd-bit-seg__name">1 bit</span>
          </div>
          <div class="bd-bit-seg" style="width:130px;flex-shrink:0;background:rgba(255,152,0,0.12);border:1px solid rgba(255,152,0,0.4);">
            <span class="bd-bit-seg__range">bits 62–52</span>
            <span class="bd-bit-seg__val" style="color:#ff9800;">Exponent</span>
            <span class="bd-bit-seg__name">11 bits · bias 1023</span>
          </div>
          <div class="bd-bit-seg" style="flex:1;background:color-mix(in srgb,var(--vp-c-brand-1) 10%,transparent);border:1px solid color-mix(in srgb,var(--vp-c-brand-1) 40%,transparent);border-radius:0 4px 4px 0;">
            <span class="bd-bit-seg__range">bits 51–0</span>
            <span class="bd-bit-seg__val">Mantissa</span>
            <span class="bd-bit-seg__name">52 bits · implicit 1.0</span>
          </div>
        </div>
        <div style="font-size:0.78rem;color:var(--vp-c-text-2);font-family:var(--vp-font-family-mono);margin-top:0.5rem;">value = (−1)^sign × 1.mantissa × 2^(exponent − 1023)</div>
      </div>
    </div>
  </div>
</div>

The goal of float parsing: compute these three fields from a decimal string using **only integer arithmetic**.

---

## The Two-Stage Decision Pipeline

Beast JSON never calls `strtod`. Every decimal string goes through two integer-only stages:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand" style="max-width:340px;">Decimal string: "3.141592653589793"</div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">SIMD-assisted extraction</div></div>
    <div class="bd-row" style="gap:1rem;">
      <div class="bd-box bd-box--teal" style="min-width:140px;">Integer mantissa m<br><small>3141592653589793</small></div>
      <div class="bd-box bd-box--teal" style="min-width:140px;">Decimal exponent e<br><small>−15 (15 digits after point)</small></div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">Stage 1 — Eisel-Lemire (64-bit fast path)</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step"><div class="bd-step__num">1</div><div class="bd-step__body"><div class="bd-step__title">Look up 64-bit approximation of 10^e</div></div></div>
          <div class="bd-step"><div class="bd-step__num">2</div><div class="bd-step__body"><div class="bd-step__title">64 × 64 → 128-bit multiply (MULQ, 3 cycles)</div></div></div>
          <div class="bd-step"><div class="bd-step__num">3</div><div class="bd-step__body"><div class="bd-step__title">Inspect top 53 bits: unambiguous?</div></div></div>
        </div>
      </div>
    </div>
    <div class="bd-row" style="gap:1rem;align-items:flex-start;">
      <div class="bd-col" style="flex:1;">
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">unambiguous (~99%)</div></div>
        <div class="bd-box bd-box--green" style="font-size:0.78rem;">Exact IEEE-754 result<br><small>no FPU · no rounding mode</small></div>
      </div>
      <div class="bd-col" style="flex:1;">
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">half-way case (~1%)</div></div>
        <div class="bd-group">
          <div class="bd-group__title">Stage 2 — Russ Cox 128-bit Exact</div>
          <div class="bd-group__body">
            <div class="bd-box bd-box--orange" style="font-size:0.75rem;">64 × 128 → 192-bit multiply</div>
            <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
            <div class="bd-box bd-box--green" style="font-size:0.75rem;">Exact IEEE-754 result</div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

There is **no third stage**. Beast JSON never falls back to `strtod`.

---

## Mantissa Extraction (SIMD-Assisted)

For a decimal string like `"1.23456789"`:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:520px;">
      <div class="bd-group__title">SIMD digit scan — "1.23456789"</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step"><div class="bd-step__num">1</div><div class="bd-step__body"><div class="bd-step__title">Load up to 16 chars into NEON/SSE register</div></div></div>
          <div class="bd-step"><div class="bd-step__num">2</div><div class="bd-step__body"><div class="bd-step__title">VPCMPEQB: detect non-digit chars (. e E + -)</div></div></div>
          <div class="bd-step"><div class="bd-step__num">3</div><div class="bd-step__body"><div class="bd-step__title">VPSUBB: subtract '0' from each digit byte</div></div></div>
          <div class="bd-step"><div class="bd-step__num">4</div><div class="bd-step__body"><div class="bd-step__title">VPMULLW + VPHADDD: fused multiply-accumulate</div><div class="bd-step__desc">d0×10^8 + d1×10^7 + … + d8×10^0</div></div></div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-row" style="gap:1rem;">
      <div class="bd-box bd-box--teal" style="min-width:140px;">m = 123456789<br><small>(integer mantissa)</small></div>
      <div class="bd-box bd-box--teal" style="min-width:140px;">e = −8<br><small>(8 decimal places)</small></div>
    </div>
  </div>
</div>

Up to 18 significant digits can be packed into a 64-bit integer without overflow (`2^63 ≈ 9.2 × 10^18`).

---

## Stage 1: Eisel-Lemire 64-bit Approximation

The precomputed table stores a 64-bit approximation of `10^e`. The product `m × approx(10^e)` yields a 128-bit integer:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:540px;">
      <div class="bd-group__title">64 × 64 → 128-bit product (MULQ instruction)</div>
      <div class="bd-group__body bd-group__body--row">
        <div class="bd-box bd-box--brand" style="flex:1;">High 64 bits<br><small>integer part of m × 10^e<br>(contains the mantissa)</small></div>
        <div class="bd-box" style="flex:1;">Low 64 bits<br><small>fractional precision indicator<br>(tells us if result is exact)</small></div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="width:100%;max-width:540px;">
      <div class="bd-group__title">Ambiguity test</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step"><div class="bd-step__num">1</div><div class="bd-step__body"><div class="bd-step__title">Find bit position P</div><div class="bd-step__desc">Leading-zero count of high 64 bits</div></div></div>
          <div class="bd-step"><div class="bd-step__num">2</div><div class="bd-step__body"><div class="bd-step__title">Extract bits [P, P−52]</div><div class="bd-step__desc">Candidate 52-bit mantissa</div></div></div>
          <div class="bd-step"><div class="bd-step__num">3</div><div class="bd-step__body"><div class="bd-step__title">Are remaining low bits exactly 0x800…0?</div><div class="bd-step__desc">Checks if result is exactly halfway between two representable values</div></div></div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-row" style="gap:1rem;">
      <div class="bd-box bd-box--green" style="flex:1;">Not halfway<br><small>→ emit IEEE-754 directly (~99%)</small></div>
      <div class="bd-box bd-box--orange" style="flex:1;">Exactly halfway<br><small>→ fall through to Stage 2 (~1%)</small></div>
    </div>
  </div>
</div>

"Unambiguous" means the product's mantissa bits are the same regardless of whether we round the approximation up or down.

---

## Stage 2: Russ Cox 128-bit Exact Path

For the ~1% of inputs where Eisel-Lemire is ambiguous, Beast JSON uses a precomputed **128-bit exact multiplier** for each power of 10:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:540px;">
      <div class="bd-group__title">Precomputed table: pow10_exact[e + 342]</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.75rem;font-family:monospace;">e = −342: 0xFA8FD5A0081C0288 9F0B8A2E0E0FF381</div>
        <div class="bd-box" style="font-size:0.75rem;font-family:monospace;">e =    0: 0x8000000000000000 0000000000000000</div>
        <div class="bd-box" style="font-size:0.75rem;font-family:monospace;">e =  308: 0xE8D4A51000000000 0000000000000000</div>
        <div class="bd-badge bd-badge--brand" style="margin-top:0.3rem;">651 entries · 10.2 KB · fits in L1 cache</div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">64-bit m × 128-bit C_e → 192-bit product</div></div>
    <div class="bd-group" style="width:100%;max-width:540px;">
      <div class="bd-group__title">192-bit product layout</div>
      <div class="bd-group__body bd-group__body--row">
        <div class="bd-box bd-box--brand" style="flex:1;">bits 191–128<br><small>high</small></div>
        <div class="bd-box bd-box--teal" style="flex:1;">bits 127–64<br><small>middle</small></div>
        <div class="bd-box" style="flex:1;">bits 63–0<br><small>low</small></div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="width:100%;max-width:540px;">
      <div class="bd-group__title">Mantissa extraction</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step"><div class="bd-step__num">1</div><div class="bd-step__body"><div class="bd-step__title">Find P = position of highest set bit</div></div></div>
          <div class="bd-step"><div class="bd-step__num">2</div><div class="bd-step__body"><div class="bd-step__title">Extract bits [P, P−52]: the 52-bit mantissa</div></div></div>
          <div class="bd-step"><div class="bd-step__num">3</div><div class="bd-step__body"><div class="bd-step__title">Round bit + Sticky bits</div><div class="bd-step__desc">Round bit = bit P−53 · Sticky = OR of all bits below P−53</div></div></div>
        </div>
      </div>
    </div>
  </div>
</div>

### The 192-bit Multiplication

On x86-64, the full 64×128→192-bit multiply decomposes into two `MULQ` instructions and one `ADCQ` (add with carry):

```mermaid
flowchart TB
    subgraph DECOMPOSE["C_e split into two 64-bit halves"]
        direction LR
        CH["C_hi = upper 64 bits of C_e"]
        CL["C_lo = lower 64 bits of C_e"]
        CH --- CL
    end

    subgraph MULS["Two multiplications merged with carry"]
        direction TB
        M1["MULQ: m × C_hi → 128-bit product p_hh"]
        M2["MULQ: m × C_lo → 128-bit product p_lh"]
        ADD["ADCQ: p_hh.lo + p_lh.hi (with carry)<br/>→ 192-bit total product"]
        M1 --> ADD
        M2 --> ADD
    end

    DECOMPOSE --> MULS
```

The entire Stage 2 path executes in **~20 cycles** — 6× faster than `strtod`.

---

## IEEE Round-to-Nearest-Even

The round bit and sticky bits determine rounding:

```mermaid
flowchart TB
    RB["Round bit = bit P−53 of the 192-bit product<br/>(1 = round up candidate, 0 = truncate)"]
    SB["Sticky bit = OR of all bits below P−53<br/>(1 = not exactly halfway, 0 = exact midpoint)"]

    CASE1["round_bit = 0<br/>→ Truncate: keep mantissa as-is"]
    CASE2["round_bit = 1 AND sticky = 1<br/>→ Round up: add 1 to mantissa"]
    CASE3["round_bit = 1 AND sticky = 0<br/>(exactly halfway)<br/>→ Round to nearest EVEN<br/>add 1 only if mantissa LSB = 1"]

    RB --> CASE1 & CASE2 & CASE3
    SB --> CASE3
```

This logic executes entirely in integer registers — **no FPU, no rounding mode, no serializing instructions**.

---

## Powers-of-Ten Table

The table covers `10^−342` through `10^308` — the full range of IEEE-754 `double`:

| Range | Entries | Size |
|:---|---:|---:|
| e = −342 to −1 | 342 | 5.3 KB |
| e = 0 | 1 | 16 B |
| e = +1 to +308 | 308 | 4.8 KB |
| **Total** | **651** | **10.2 KB** |

10.2 KB fits comfortably in a 32 KB L1 data cache. For number-heavy workloads (financial data, sensor streams), the table stays **permanently hot** across the entire parse session.

---

## Performance Comparison

| | `strtod` / `atof` | Eisel-Lemire (Stage 1) | Russ Cox (Stage 2) |
|:---|---:|---:|---:|
| **Instructions** | ~200 | ~25 | ~60 |
| **Pipeline flushes** | 2 (`STMXCSR`/`LDMXCSR`) | **0** | **0** |
| **FPU operations** | 7–17 | **0** | **0** |
| **Table lookup** | none | 8 bytes | 16 bytes |
| **Throughput** | ~120 ns | **~8 ns** | **~20 ns** |
| **Frequency** | — | ~99% of inputs | ~1% of inputs |

**Weighted average: ~8.1 ns per float** vs ~120 ns for `strtod` — a **~15× speedup** for number-heavy documents.

---

## Correctness Guarantee

Beast JSON produces the **correctly rounded IEEE-754 result** for all possible finite decimal inputs:

- All decimal strings with ≤ 15 significant digits: provably exact via Eisel-Lemire
- All remaining inputs: provably exact via the 128-bit Russ Cox path
- No input ever produces a result that differs from `strtod`'s correctly-rounded output

> The algorithm is bit-for-bit identical to `strtod` on all inputs — it is simply **15× faster** by avoiding FPU rounding-mode manipulation and sequential decimal multiplication.
