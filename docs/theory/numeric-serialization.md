# Numeric Serialization: Schubfach + yy-itoa

Beast JSON uses two state-of-the-art algorithms for converting numbers to their ASCII
representations: **yy-itoa** for integers and **Schubfach** for floating-point.
Together they are 2–3× faster than `std::to_chars` on GCC/Clang and completely
eliminate `printf`/`snprintf`.

Both algorithms originate from the open-source **[yyjson](https://github.com/ibireme/yyjson)**
library (MIT License) by ibireme / Y. Yuan, with the floating-point core tracing back to
**Raffaello Giulietti's** original Schubfach paper (2020).

---

## Why `std::to_chars` Is Not Enough

`std::to_chars` is excellent — but it has inherent costs:

| Factor | `std::to_chars` | Beast bj_nc |
|:---|:---|:---|
| **Integer** | Generic; handles all widths uniformly | Width-specialized dispatch — 32 vs 64-bit separate paths |
| **Float** | Implementation-defined; libstdc++ uses Ryu or Dragon4 | Always Schubfach (consistent, tuned) |
| **Division** | One `div`/`mod` per 1–2 digits | **Zero division** — multiply-shift trick |
| **Apple Clang** | `std::to_chars(double)` unsupported before macOS 13 | Always available — pure C++20 |
| **Trailing zeros** | Preserved (default) | Trimmed automatically (shortest decimal) |

---

## Part 1 — yy-itoa: Integer Serialization Without Division

Converting an integer to decimal typically looks like this:

```cpp
// Naive — one division per digit
while (n > 0) {
    *--p = '0' + (n % 10);
    n /= 10;
}
```

Each `%` and `/` is a hardware division instruction (~20–40 cycles on modern x86). For a
6-digit number, that's 12 division operations.

### The 2-Digit Lookup Table

yy-itoa eliminates all divisions using a **200-byte lookup table** that stores the
ASCII representation of every 2-digit pair from `00` to `99`:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:540px;">
      <div class="bd-group__title">char_table[200] — 100 pairs, 2 bytes each</div>
      <div class="bd-group__body">
        <div class="bd-tape-strip" style="flex-wrap:wrap;gap:4px;justify-content:flex-start;">
          <div class="bd-tape-cell bd-tape-cell--int" style="min-width:60px;"><span class="bd-tape-cell__idx">[0]</span><span class="bd-tape-cell__val">'0','0'</span></div>
          <div class="bd-tape-cell bd-tape-cell--int" style="min-width:60px;"><span class="bd-tape-cell__idx">[2]</span><span class="bd-tape-cell__val">'0','1'</span></div>
          <div class="bd-tape-cell bd-tape-cell--int" style="min-width:60px;"><span class="bd-tape-cell__idx">[4]</span><span class="bd-tape-cell__val">'0','2'</span></div>
          <div class="bd-tape-cell" style="min-width:60px;"><span class="bd-tape-cell__idx">…</span><span class="bd-tape-cell__val">…</span></div>
          <div class="bd-tape-cell bd-tape-cell--key" style="min-width:60px;"><span class="bd-tape-cell__idx">[98]</span><span class="bd-tape-cell__val">'4','9'</span></div>
          <div class="bd-tape-cell" style="min-width:60px;"><span class="bd-tape-cell__idx">…</span><span class="bd-tape-cell__val">…</span></div>
          <div class="bd-tape-cell bd-tape-cell--bool" style="min-width:60px;"><span class="bd-tape-cell__idx">[198]</span><span class="bd-tape-cell__val">'9','9'</span></div>
        </div>
      </div>
    </div>
  </div>
</div>

Instead of dividing by 10, the algorithm splits the number into two 2-digit groups using
a **multiply-shift** operation — equivalent to dividing by 100, but using only a
multiplication and a right shift (both single-cycle instructions):

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:560px;">
      <div class="bd-group__title">Processing the number 9731 in two steps</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step">
            <div class="bd-step__num">1</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Split via multiply-shift</div>
              <div class="bd-step__desc">
                <code>hi = (9731 × 5243) >> 19 = 97</code> (= 9731 / 100)<br>
                <code>lo = 9731 − 97 × 100 = 31</code>
              </div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">2</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Table lookup — zero branches</div>
              <div class="bd-step__desc">
                <code>memcpy(out, &amp;char_table[97 × 2], 2)</code> → writes <code>'9','7'</code><br>
                <code>memcpy(out+2, &amp;char_table[31 × 2], 2)</code> → writes <code>'3','1'</code>
              </div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">3</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Result</div>
              <div class="bd-step__desc">Buffer contains <code>"9731"</code> — <strong>zero division instructions used</strong></div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

### Why `(n × 5243) >> 19` equals `n / 100`

This is a compiler-known technique called **integer division by multiplication**:

$$\left\lfloor \frac{n}{100} \right\rfloor = \left\lfloor \frac{n \times \lceil 2^{19}/100 \rceil}{2^{19}} \right\rfloor = \left\lfloor \frac{n \times 5243}{524288} \right\rfloor$$

The magic constant `5243 = ⌈2¹⁹ / 100⌉`. The right-shift by 19 divides by 2¹⁹ = 524288.
This identity holds exactly for all $n < 10000$. For larger numbers, the algorithm
recurses — splitting 64-bit values into 32-bit halves, then 4-digit groups.

### Width Specialization

Beast JSON dispatches to separate routines by type:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:560px;">
      <div class="bd-group__title"><code>bj_nc::to_chars</code> dispatch tree</div>
      <div class="bd-group__body">

<div style="font-family:var(--vp-font-family-mono);font-size:0.82rem;line-height:1.7;padding:0.5rem 0;">

```
to_chars(buf, v)
├── uint32_t  → 1-4 digit  fast-path  (≤4 branches, no 64-bit mul)
│   ├── < 10        → 1 digit direct write
│   ├── < 100       → 1 table lookup (2 bytes)
│   ├── < 10 000    → 1 mul-shift + 2 lookups
│   └── ≥ 10 000    → split into hi/lo, each ≤ 10 000
├── int32_t   → sign prefix + uint32_t path
├── uint64_t  → split into 2 × uint32_t halves
│   └── each half → uint32_t path above
└── int64_t   → sign prefix + uint64_t path
```

</div>
      </div>
    </div>
  </div>
</div>

The `uint32_t` path is also used for values whose absolute value fits in 32 bits
(the common case for port numbers, counts, small IDs), keeping the instruction count
as low as 3–5 for values under 10,000.

---

## Part 2 — Schubfach: Floating-Point Serialization

Converting `3.141592653589793` (a `double`) to its string representation is a
non-trivial numerical problem. The requirement is:

> **Shortest round-trip:** produce the shortest decimal string $d$ such that
> parsing $d$ with any conformant parser gives back the **exact same `double`**.

### Historical Context

| Algorithm | Year | Notes |
|:---|:---|:---|
| **Dragon4** | 1990 | Steele & White — correct but slow (bignum arithmetic) |
| **Grisu2** | 2010 | Loitsch — fast but sometimes produces non-shortest output |
| **Grisu3** | 2010 | Loitsch — shortest but needs Dragon4 fallback for ~0.5% of values |
| **Ryu** | 2018 | Mori — shortest, no fallback, 128-bit pow10 tables |
| **Schubfach** | 2020 | Giulietti — shortest, no fallback, simpler branching than Ryu |

Beast JSON uses **Schubfach** via the yyjson port, which adds optimizations for
the common case (numbers with few significant digits, no exponent).

### Core Idea: The Schubfach Interval

Every `double` $v$ has a "acceptance interval" $[v^-, v^+]$ of decimal values that
all round back to $v$ under IEEE 754. Schubfach finds the shortest decimal inside
this interval.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:600px;">
      <div class="bd-group__title">Finding the shortest decimal for a double</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step">
            <div class="bd-step__num">1</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Extract IEEE 754 components</div>
              <div class="bd-step__desc">
                Bit-cast <code>double</code> → <code>uint64_t</code> → separate <code>sig</code> (52-bit mantissa) and <code>exp</code> (11-bit exponent).<br>
                No floating-point arithmetic needed after this step.
              </div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">2</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Compute decimal exponent k</div>
              <div class="bd-step__desc">
                <code>k = ceil(exp × log₁₀(2))</code> — determines how many digits are needed.<br>
                Pre-computed as an integer via: <code>k = (exp × 1233 + 1023) >> 12</code>
              </div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">3</div>
            <div class="bd-step__body">
              <div class="bd-step__title">128-bit pow10 table lookup</div>
              <div class="bd-step__desc">
                Look up precomputed <code>10^k</code> as a 128-bit fixed-point integer (1336 entries, 16 KB).<br>
                Multiply <code>sig × pow10[k]</code> → 128-bit product → extract decimal significand.
              </div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">4</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Trim trailing zeros</div>
              <div class="bd-step__desc">
                Remove trailing zeros from the decimal significand while the value still round-trips.<br>
                Result: <em>shortest</em> decimal string. <code>3.0</code> prints as <code>"3"</code>, not <code>"3.00000000000000"</code>.
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

### The 128-bit Multiplication (No Hardware Support Needed)

The key computation is `sig × pow10[k]` — a 64×64→128 bit multiplication.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:600px;">
      <div class="bd-group__title">128-bit integer multiply: platform dispatch</div>
      <div class="bd-group__body">
        <div class="bd-row" style="gap:1rem;align-items:stretch;">
          <div class="bd-group" style="flex:1;margin:0;">
            <div class="bd-group__title" style="font-size:0.7rem;">GCC / Clang</div>
            <div class="bd-group__body">
              <div class="bd-box bd-box--teal" style="font-size:0.78rem;">
                <code>__uint128_t</code><br>
                <small>Compiler emits <code>MUL</code>+<code>MULH</code> pair</small>
              </div>
            </div>
          </div>
          <div class="bd-group" style="flex:1;margin:0;">
            <div class="bd-group__title" style="font-size:0.7rem;">MSVC x86-64</div>
            <div class="bd-group__body">
              <div class="bd-box bd-box--orange" style="font-size:0.78rem;">
                <code>_umul128()</code> + <code>__umulh()</code><br>
                <small>Intrinsic pair, 2 cycles</small>
              </div>
            </div>
          </div>
          <div class="bd-group" style="flex:1;margin:0;">
            <div class="bd-group__title" style="font-size:0.7rem;">MSVC ARM64</div>
            <div class="bd-group__body">
              <div class="bd-box bd-box--purple" style="font-size:0.78rem;">
                <code>_umul128()</code> + <code>__umulh()</code><br>
                <small>ARM64 <code>UMULH</code> instruction</small>
              </div>
            </div>
          </div>
          <div class="bd-group" style="flex:1;margin:0;">
            <div class="bd-group__title" style="font-size:0.7rem;">32-bit / Generic</div>
            <div class="bd-group__body">
              <div class="bd-box" style="font-size:0.78rem;">
                4 × 32-bit mul<br>
                <small>Scalar long multiply</small>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

### Concrete Example: `3.14`

```
double 3.14 → IEEE 754 bits:
  sign=0, exp=1024 (biased), sig=0x91EB851EB851F (52 bits)

Step 1: Extract
  sig_bin = 0xC8F5C28F5C28F  (normalized: prepend implicit 1-bit)
  exp_bin = 1 (actual exponent: 2^1 × 0.c8f5c...)

Step 2: k = ceil(1 × log10(2)) = 1
  → use pow10 table entry for k=1

Step 3: sig × pow10[1] → 128-bit product → sig_dec = 314
  exp_dec = -2  (means 314 × 10^-2 = 3.14)

Step 4: Trim trailing zeros
  314 has no trailing zeros → output "3.14"
```

Compare: `printf("%.17g", 3.14)` → `"3.1400000000000001"` (17 significant digits).
Schubfach produces `"3.14"` — 14 fewer bytes, same information.

### Special Value Handling

Beast JSON's `bj_nc::to_chars(double)` maps non-finite values to JSON-safe strings:

```cpp
if (std::isinf(v) || std::isnan(v))  →  writes "null"
-0.0                                  →  writes "0"   (positive zero form)
```

This is intentional — JSON has no syntax for infinity or NaN, and `null` is the
RFC 8259-compliant fallback.

---

## Part 3 — FastWriter: Struct Serialization Architecture

Serializing a struct with N fields traditionally causes up to **3N string operations**:

```
write key "id"    → string append (potential realloc)
write ":"         → string append
write value       → string append (potential realloc)
... repeat for each field
```

**FastWriter** pre-grows the destination `std::string` by an estimated capacity and
writes directly into the spare bytes via raw pointer:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:560px;">
      <div class="bd-group__title">FastWriter — pre-grown buffer with raw pointer writes</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step">
            <div class="bd-step__num">1</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Constructor: reserve capacity</div>
              <div class="bd-step__desc">
                <code>buf.reserve(buf.size() + estimated)</code> — one allocation,<br>
                <code>write_ptr = buf.data() + buf.size()</code> — raw pointer into spare bytes
              </div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">2</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Field writes: raw memcpy</div>
              <div class="bd-step__desc">
                Key bytes, separator <code>:</code>, value bytes all written via <code>memcpy</code> to <code>write_ptr</code>.<br>
                <strong>Zero reallocation. Zero bounds checks.</strong>
              </div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num">3</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Destructor: commit written bytes</div>
              <div class="bd-step__desc">
                <code>buf.resize(actual_size)</code> — the string's length is updated<br>
                to reflect exactly what was written. Surplus capacity is kept for reuse.
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

The `HasBeastJsonFW<T, W>` concept lets nested structs route through the **same**
FastWriter instance — so a struct containing other structs still costs only one
pre-grow and one commit, regardless of nesting depth.

---

## Combined Serialize Pipeline

Putting it all together, here is what happens when you call `beast::write(my_struct)`:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand">beast::write(my_struct)</div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="width:100%;max-width:580px;">
      <div class="bd-group__title">FastWriter — pre-reserve, raw-pointer writes</div>
      <div class="bd-group__body">
        <div class="bd-row" style="gap:0.75rem;">
          <div class="bd-group" style="flex:1;margin:0;">
            <div class="bd-group__title" style="font-size:0.7rem;">Opening brace + keys</div>
            <div class="bd-group__body">
              <div class="bd-box bd-box--teal" style="font-size:0.78rem;">
                Literal bytes<br><code>memcpy</code> to ptr
              </div>
            </div>
          </div>
          <div class="bd-group" style="flex:1;margin:0;">
            <div class="bd-group__title" style="font-size:0.7rem;">Integer values</div>
            <div class="bd-group__body">
              <div class="bd-box bd-box--green" style="font-size:0.78rem;">
                <strong>yy-itoa</strong><br>multiply-shift<br>table lookup
              </div>
            </div>
          </div>
          <div class="bd-group" style="flex:1;margin:0;">
            <div class="bd-group__title" style="font-size:0.7rem;">Float values</div>
            <div class="bd-group__body">
              <div class="bd-box bd-box--purple" style="font-size:0.78rem;">
                <strong>Schubfach</strong><br>128-bit mul<br>trailing trim
              </div>
            </div>
          </div>
          <div class="bd-group" style="flex:1;margin:0;">
            <div class="bd-group__title" style="font-size:0.7rem;">String values</div>
            <div class="bd-group__body">
              <div class="bd-box bd-box--orange" style="font-size:0.78rem;">
                SSE2/NEON<br>escape scan<br>bulk copy
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-box bd-box--brand">std::string output — <strong>zero reallocation in hot path</strong></div>
  </div>
</div>

---

## Attribution

The `bj_nc` namespace in `beast_json.hpp` is a clean-room port and adaptation of
algorithms from two sources:

### yyjson (MIT License)

```
Copyright (c) 2020 YaoYuan <ibireme@gmail.com>
Source: https://github.com/ibireme/yyjson

Algorithms used:
  - yy-itoa (integer to ASCII)
  - Schubfach dtoa port (double to ASCII)
```

Both algorithms appear in `yyjson.c` under the MIT license. Beast JSON's port
preserves the MIT-compatible license terms by maintaining this attribution in both
source and documentation.

### Raffaello Giulietti — Schubfach

The floating-point core implements the algorithm described in:

> **Raffaello Giulietti**, *"The Schubfach way to render doubles,"* 2020.
> [PDF (Google Docs)](https://drive.google.com/file/d/1IEeATSVnEE6TkrHlCYNY2GjaraBjOT4f)

The algorithm is named after the German word *Schubfach* (drawer) — each decimal
digit is placed into its "drawer" by the 128-bit computation.

---

## See Also

- [Acknowledgments & References](/guide/acknowledgments) — full attribution list
- [SIMD Acceleration](/theory/simd) — how parsing is accelerated
- [Nexus Fusion](/theory/nexus-fusion) — zero-tape struct mapping
