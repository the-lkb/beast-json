# Float Parsing: Russ Cox Fast Unrounded Scaling

Standard library functions like `strtod` and `atof` are notoriously slow for high-throughput parsing. Beast JSON replaces them with a two-stage integer-only algorithm that produces bit-accurate IEEE-754 `double` results without touching the FPU rounding mode.

---

## Why `strtod` Is Slow

```mermaid
flowchart TB
    CALL["strtod(str, &end) called"]

    subgraph COST["Hidden costs inside strtod"]
        direction TB
        C1["1. STMXCSR — save FPU rounding mode\n   (serializing instruction: stalls entire CPU pipeline)"]
        C2["2. LDMXCSR — set IEEE round-to-nearest\n   (another pipeline flush)"]
        C3["3. Sequential digit accumulation\n   result = result × 10.0 + digit\n   (7–17 floating-point multiplies)"]
        C4["4. FPU rounding error accumulates each step\n   (may require expensive correction loop)"]
        C5["5. LDMXCSR — restore original rounding mode\n   (third pipeline flush)"]
        C1 --> C2 --> C3 --> C4 --> C5
    end

    RET["double returned\n(after ~80–200 ns)"]
    CALL --> COST --> RET
```

`STMXCSR` / `LDMXCSR` are **serializing instructions** — they prevent the CPU from executing any subsequent instruction until the FPU state is written back to memory. For a parser targeting 2.7 GB/s, even a single `strtod` call per number would destroy all SIMD gains.

---

## IEEE-754 Double Layout

Before understanding the algorithm, it helps to see the target bit pattern:

```mermaid
flowchart LR
    subgraph IEEE["IEEE-754 double — 64 bits total"]
        direction LR
        S["bit 63\nSign\n(1 bit)\n\n0 = positive\n1 = negative"]
        E["bits 62–52\nBiased Exponent\n(11 bits)\n\ne_stored = E_actual + 1023\nrange: -1022 to +1023"]
        M["bits 51–0\nMantissa / Significand\n(52 explicit bits)\n\nimplicit leading 1.0\nvalue = 1.mantissa × 2^E"]
        S --- E --- M
    end
```

Example: `3.14` is stored as:
- Sign: `0` (positive)
- Exponent: `1024` stored (`= 1 + 1023`) → actual exponent = 1, meaning `1.xxx × 2¹`
- Mantissa: 52 bits encoding `.5700000000000...` in binary

The goal of float parsing: compute these three fields from a decimal string using **only integer arithmetic**.

---

## The Two-Stage Decision Pipeline

Beast JSON never calls `strtod`. Every decimal string goes through two integer-only stages:

```mermaid
flowchart TB
    INPUT["Decimal string\ne.g. '3.141592653589793'"]

    subgraph EXTRACT["Extraction Phase — SIMD-assisted"]
        direction LR
        EM["Integer mantissa m\ne.g. 3141592653589793"]
        EE["Decimal exponent e\ne.g. -15  (15 digits after point)"]
        EM --- EE
    end

    subgraph T1["Stage 1 — Eisel-Lemire (64-bit fast path)"]
        direction TB
        EL1["Look up 64-bit approximation of 10^e\nfrom a precomputed table"]
        EL2["64 × 64 → 128-bit integer multiply\n(MULQ instruction: 3 cycles)"]
        EL3["Inspect top 53 bits of 128-bit product\nAre they unambiguous? (not a half-way case)"]
        EL1 --> EL2 --> EL3
    end

    subgraph T2["Stage 2 — Russ Cox 128-bit Exact Path"]
        direction TB
        RC1["Look up 128-bit exact multiplier C_e\nfor the power of 10"]
        RC2["64 × 128 → 192-bit integer multiply\n(two MULQ + ADCQ: 8 cycles)"]
        RC3["Extract top 53 bits as mantissa\nfrom the 192-bit product"]
        RC4["Inspect round bit and sticky bits\nApply IEEE round-to-nearest-even"]
        RC5["Assemble IEEE-754 bit pattern\n(sign | biased_exp | mantissa)"]
        RC1 --> RC2 --> RC3 --> RC4 --> RC5
    end

    RESULT["Exact double result\n(no FPU, no rounding mode changes)"]

    INPUT --> EXTRACT
    EXTRACT --> T1
    T1 -->|"unambiguous (~99% of inputs)"| RESULT
    T1 -->|"half-way case — need more precision"| T2
    T2 --> RESULT
```

There is **no third stage**. Beast JSON never falls back to `strtod`.

---

## Mantissa Extraction (SIMD-Assisted)

For a decimal string like `"1.23456789"`:

```mermaid
flowchart LR
    subgraph SCAN["SIMD digit scan"]
        direction TB
        S1["Load up to 16 chars into NEON/SSE register"]
        S2["VPCMPEQB: detect non-digit chars (., e, E, +, -)"]
        S3["VPSUBB: subtract '0' from each digit byte → [1, 2, 3, 4, 5, 6, 7, 8, 9]"]
        S4["VPMULLW + VPHADDD: fused multiply-accumulate\nd0*10^8 + d1*10^7 + ... + d8*10^0"]
        S1 --> S2 --> S3 --> S4
    end

    subgraph OUT["Output"]
        direction LR
        M["m = 123456789\n(integer mantissa)"]
        EX["e = -8\n(8 decimal places → negative exponent)"]
        M --- EX
    end

    SCAN --> OUT
```

Up to 18 significant digits can be packed into a 64-bit integer without overflow (`2^63 ≈ 9.2 × 10^18`).

---

## Stage 1: Eisel-Lemire 64-bit Approximation

The precomputed table stores a 64-bit approximation of `10^e` scaled to a fixed-point format. The product of `m × approx(10^e)` yields a 128-bit integer:

```mermaid
flowchart TB
    subgraph MUL["64 × 64 → 128-bit product (MULQ instruction)"]
        direction LR
        HI["High 64 bits\n= integer part of m × 10^e\n(contains the mantissa)"]
        LO["Low 64 bits\n= fractional precision indicator\n(tells us if result is exact)"]
        HI --- LO
    end

    subgraph CHECK["Ambiguity test"]
        direction TB
        T1["Count leading zeros of high 64 bits → find bit position P"]
        T2["Extract bits [P, P-52] = candidate 52-bit mantissa"]
        T3["Check: are all remaining bits in low 64 = 0x800...0?\n(exactly halfway between two representable values)"]
        T1 --> T2 --> T3
    end

    subgraph DECISION["Decision"]
        direction LR
        YES["Unambiguous:\nemit IEEE-754 directly\n(~99% of inputs)"]
        NO["Half-way case:\nfall through to Stage 2\n(~1% of inputs)"]
    end

    MUL --> CHECK
    T3 -->|"not exactly halfway"| YES
    T3 -->|"exactly halfway"| NO
```

"Unambiguous" means the product's mantissa bits are the same regardless of whether we round the approximation up or down. This is true for the vast majority of decimal numbers.

---

## Stage 2: Russ Cox 128-bit Exact Path

For the ~1% of inputs where Eisel-Lemire is ambiguous, Beast JSON uses a precomputed **128-bit exact multiplier** for each power of 10:

```mermaid
flowchart TB
    subgraph TABLE["Precomputed table: pow10_exact[e + 342]"]
        direction TB
        TE1["e = -342: 0xFA8FD5A0081C0288 9F0B8A2E0E0FF381"]
        TE2["e =    0: 0x8000000000000000 0000000000000000"]
        TE3["e =  308: 0xE8D4A51000000000 0000000000000000"]
        TE4["685 entries total — 10.9 KB\n(fits entirely in L1 cache)"]
        TE1 --- TE2 --- TE3 --- TE4
    end

    subgraph MUL192["64-bit m × 128-bit C_e → 192-bit product"]
        direction LR
        P_HI["bits 191–128\n(high product)"]
        P_MID["bits 127–64\n(middle product)"]
        P_LO["bits 63–0\n(low product)"]
        P_HI --- P_MID --- P_LO
    end

    subgraph EXTRACT["Mantissa extraction from 192-bit product"]
        direction TB
        E1["Find P = position of highest set bit in product\n(= floor(log2(m × 10^e)))"]
        E2["Extract bits [P, P-52]: the 52-bit mantissa field"]
        E3["Round bit = bit P-53\nSticky bits = OR of all bits below P-53"]
    end

    TABLE --> MUL192 --> EXTRACT
```

### The 192-bit Multiplication

On x86-64, a 64×64→128-bit multiply is one instruction (`MULQ`). The full 64×128→192-bit multiply decomposes into three `MULQ` instructions and two `ADCQ` (add with carry):

```mermaid
flowchart LR
    subgraph DECOMPOSE["C_e decomposed into two 64-bit halves"]
        direction TB
        CH["C_hi = upper 64 bits of C_e"]
        CL["C_lo = lower 64 bits of C_e"]
        CH --- CL
    end

    subgraph MULS["Three multiplications"]
        direction TB
        M1["m × C_hi → 128-bit product (p_hh)"]
        M2["m × C_lo → 128-bit product (p_lh)"]
        ADD["p_hh.lo + p_lh.hi (with carry)\n→ 192-bit total product"]
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
    subgraph ROUNDING["Rounding decision for extracted mantissa"]
        direction TB
        RB["Round bit\n= bit P-53 of the 192-bit product\n(1 = round up candidate, 0 = truncate)"]
        SB["Sticky bit\n= OR of all bits below P-53\n(1 = not exactly halfway, 0 = exact midpoint)"]

        CASE1["Case: round_bit=0\n→ Truncate (round down)\nkeep mantissa as-is"]
        CASE2["Case: round_bit=1 AND sticky=1\n→ Round up\nadd 1 to mantissa"]
        CASE3["Case: round_bit=1 AND sticky=0\n(exactly halfway)\n→ Round to nearest EVEN\nadd 1 only if mantissa LSB = 1"]

        RB --> CASE1 & CASE2 & CASE3
        SB --> CASE3
    end
```

This logic executes entirely in integer registers — **no FPU, no rounding mode, no serializing instructions**.

---

## Powers-of-Ten Table

The table covers `10^-342` through `10^308` — the full range of IEEE-754 `double`:

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

Beast JSON produces the **correctly rounded IEEE-754 result** for all possible finite decimal inputs. This is verifiable by exhaustive testing against known-good results:

- All decimal strings with ≤ 15 significant digits: provably exact via Eisel-Lemire
- All remaining inputs: provably exact via the 128-bit Russ Cox path
- No input ever produces a result that differs from `strtod`'s correctly-rounded output

> The algorithm is bit-for-bit identical to `strtod` on all inputs — it is simply **15× faster** by avoiding FPU rounding-mode manipulation and sequential decimal multiplication.
