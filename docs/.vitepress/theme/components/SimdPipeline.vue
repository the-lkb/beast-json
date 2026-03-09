<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'

// 20-byte input for visualization
const INPUT = '{ "name": "Alice" }'

// Which chars are structural
function isStructural(c: string) {
  return '{}[]:"\\,'.includes(c)
}

// Pre-compute metadata for each byte
const bytes = Array.from(INPUT).map((c, i) => ({
  i,
  c,
  structural: isStructural(c),
  // inside string: between the 2nd and 3rd quote (name → value region)
  inString: i >= 2 && i <= 8 || i >= 10 && i <= 16,
}))

// True structural (not inside string, but show both states)
const cleanStructural = bytes.map(b => b.structural && !b.inString)

const STAGES = [
  {
    id: 'load',
    label: 'Stage 1a — Load',
    desc: 'VMOVDQU64 loads 64 bytes into zmm0 in 1 cycle. We show the first 20 bytes of this window.',
    highlight: (_i: number) => true,
    accent: '#0097a7',
  },
  {
    id: 'mask',
    label: 'Stage 1b — Structural Mask',
    desc: '8× VPCMPEQB compares all bytes against structural chars in parallel → 64-bit bitmask.',
    highlight: (i: number) => bytes[i].structural,
    accent: '#ff9800',
  },
  {
    id: 'string',
    label: 'Stage 1c — Quote Masking (PCLMULQDQ)',
    desc: 'Prefix-XOR via carryless multiply suppresses structural chars inside string literals.',
    highlight: (i: number) => bytes[i].inString,
    accent: '#e91e63',
  },
  {
    id: 'clean',
    label: 'Stage 1d — Clean Structural Mask',
    desc: 'clean = structural AND NOT in_string. Only real structural positions survive.',
    highlight: (i: number) => cleanStructural[i],
    accent: '#4caf50',
  },
  {
    id: 'stage2',
    label: 'Stage 2 — Tape Generation',
    desc: 'TZCNT iterates set bits. One TapeNode written per structural char. ~5-15% of input visited.',
    highlight: (i: number) => cleanStructural[i],
    accent: '#9c27b0',
  },
]

const stageIdx = ref(0)
const current = computed(() => STAGES[stageIdx.value])

function next() { stageIdx.value = Math.min(STAGES.length - 1, stageIdx.value + 1) }
function prev() { stageIdx.value = Math.max(0, stageIdx.value - 1) }

// Bit display for the mask stages
const maskBits = computed(() => {
  if (stageIdx.value === 1) return bytes.map(b => b.structural ? 1 : 0)
  if (stageIdx.value === 2) return bytes.map(b => b.inString   ? 1 : 0)
  if (stageIdx.value >= 3) return cleanStructural.map(v => v ? 1 : 0)
  return null
})

// TapeNodes emitted (for stage 2)
const tapeNodes = computed(() => {
  if (stageIdx.value < 4) return []
  const result: { char: string; tag: string; idx: number }[] = []
  bytes.forEach((b, i) => {
    if (!cleanStructural[i]) return
    const tag =
      b.c === '{' ? 'OBJ_START' :
      b.c === '}' ? 'OBJ_END'   :
      b.c === '"' ? (result.length === 0 ? 'KEY' : 'STRING') :
      b.c === ':' ? 'COLON'     : b.c
    result.push({ char: b.c, tag, idx: i })
  })
  return result
})
</script>

<template>
  <div class="simd-wrap">
    <!-- Stage tabs -->
    <div class="simd-stages">
      <div
        v-for="(s, si) in STAGES"
        :key="s.id"
        class="simd-stage-dot"
        :class="{ active: si === stageIdx, done: si < stageIdx }"
        :style="si <= stageIdx ? { background: s.accent, boxShadow: `0 0 8px ${s.accent}88` } : {}"
        @click="stageIdx = si"
      >
        <span v-if="si < stageIdx">✓</span>
        <span v-else>{{ si + 1 }}</span>
      </div>
      <div
        v-for="si in STAGES.length - 1"
        :key="'line-' + si"
        class="simd-stage-line"
        :class="{ done: si <= stageIdx }"
        :style="si <= stageIdx ? { background: STAGES[si - 1].accent } : {}"
      ></div>
    </div>

    <!-- Stage header -->
    <div class="simd-header" :style="{ '--acc': current.accent }">
      <span class="simd-header__num">Stage {{ stageIdx + 1 }}/{{ STAGES.length }}</span>
      <span class="simd-header__label">{{ current.label }}</span>
    </div>
    <p class="simd-desc">{{ current.desc }}</p>

    <!-- Byte grid -->
    <div class="simd-grid-wrap">
      <div class="simd-grid">
        <div
          v-for="b in bytes"
          :key="b.i"
          class="simd-byte"
          :class="{
            'simd-byte--lit':    current.highlight(b.i),
            'simd-byte--dimmed': stageIdx > 0 && !current.highlight(b.i),
          }"
          :style="current.highlight(b.i) ? { '--acc': current.accent } : {}"
        >
          <span class="simd-byte__idx">{{ b.i }}</span>
          <span class="simd-byte__char">{{ b.c === ' ' ? '·' : b.c }}</span>
        </div>
      </div>
    </div>

    <!-- Bitmask row (stages 1-3) -->
    <Transition name="fade">
      <div v-if="maskBits" class="simd-mask-row">
        <span class="simd-mask-label">mask:</span>
        <div class="simd-bits">
          <span
            v-for="(bit, bi) in maskBits"
            :key="bi"
            class="simd-bit"
            :class="{ 'simd-bit--one': bit === 1 }"
            :style="bit === 1 ? { color: current.accent } : {}"
          >{{ bit }}</span>
        </div>
      </div>
    </Transition>

    <!-- TapeNode output (stage 2) -->
    <Transition name="fade">
      <div v-if="tapeNodes.length" class="simd-tape-out">
        <span class="simd-tape-out__label">TapeNodes emitted ({{ tapeNodes.length }}/{{ bytes.length }} bytes):</span>
        <div class="simd-tape-out__nodes">
          <div
            v-for="(tn, ti) in tapeNodes"
            :key="ti"
            class="simd-tape-node"
            :style="{ animationDelay: `${ti * 80}ms` }"
          >
            <span class="simd-tape-node__char">'{{ tn.char }}'</span>
            <span class="simd-tape-node__tag">{{ tn.tag }}</span>
          </div>
        </div>
      </div>
    </Transition>

    <!-- Nav -->
    <div class="simd-nav">
      <button class="simd-btn" :disabled="stageIdx === 0" @click="prev">← Prev</button>
      <span class="simd-nav__hint">{{ stageIdx + 1 }} / {{ STAGES.length }}</span>
      <button class="simd-btn" :disabled="stageIdx === STAGES.length - 1" @click="next">Next →</button>
    </div>
  </div>
</template>

<style scoped>
.simd-wrap {
  border-radius: 12px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  margin: 2rem 0;
  overflow: hidden;
  font-family: var(--vp-font-family-mono);
}

/* ── stage dots progress ── */
.simd-stages {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1rem 1rem 0.25rem;
  position: relative;
  gap: 0;
}
/* interleave dots and lines using a wrapper grid approach */
.simd-stages {
  display: grid;
  grid-template-columns: repeat(9, auto);
  align-items: center;
  padding: 1rem 2rem 0.5rem;
}
.simd-stage-dot {
  width: 28px;
  height: 28px;
  border-radius: 50%;
  border: 2px solid var(--vp-c-divider);
  background: var(--vp-c-bg-mute);
  color: var(--vp-c-text-2);
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 0.75rem;
  font-weight: 700;
  cursor: pointer;
  transition: all 0.25s;
  z-index: 1;
}
.simd-stage-dot.active { color: #fff; border-color: transparent; }
.simd-stage-line {
  height: 2px;
  background: var(--vp-c-divider);
  flex: 1;
  min-width: 20px;
  transition: background 0.35s;
}

/* ── header ── */
.simd-header {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.75rem 1.25rem 0;
  flex-wrap: wrap;
}
.simd-header__num {
  font-size: 0.68rem;
  color: var(--vp-c-text-3);
  text-transform: uppercase;
  letter-spacing: 0.07em;
}
.simd-header__label {
  font-size: 0.9rem;
  font-weight: 700;
  color: var(--acc, var(--vp-c-brand-1));
}
.simd-desc {
  margin: 0.35rem 1.25rem 0.75rem;
  font-size: 0.82rem;
  color: var(--vp-c-text-2);
  font-family: var(--vp-font-family-base);
  line-height: 1.5;
}

/* ── byte grid ── */
.simd-grid-wrap {
  padding: 0 1rem;
  overflow-x: auto;
  -webkit-overflow-scrolling: touch;
}
.simd-grid {
  display: flex;
  gap: 4px;
  padding: 0.5rem 0.25rem;
  width: max-content;
  min-width: 100%;
}
.simd-byte {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  padding: 0.4rem 0.35rem;
  border-radius: 5px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-mute);
  min-width: 30px;
  transition: all 0.25s ease;
}
.simd-byte--lit {
  border-color: var(--acc, var(--vp-c-brand-1));
  background: color-mix(in srgb, var(--acc, var(--vp-c-brand-1)) 14%, transparent);
  box-shadow: 0 0 8px color-mix(in srgb, var(--acc, var(--vp-c-brand-1)) 40%, transparent);
}
.simd-byte--dimmed {
  opacity: 0.3;
}
.simd-byte__idx  { font-size: 0.55rem; color: var(--vp-c-text-3); }
.simd-byte__char { font-size: 0.85rem; font-weight: 700; color: var(--vp-c-text-1); }
.simd-byte--lit .simd-byte__char { color: var(--acc, var(--vp-c-brand-1)); }

/* ── mask row ── */
.simd-mask-row {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.25rem 1.25rem 0.5rem;
  overflow-x: auto;
}
.simd-mask-label {
  font-size: 0.72rem;
  color: var(--vp-c-text-2);
  flex-shrink: 0;
}
.simd-bits {
  display: flex;
  gap: 4px;
}
.simd-bit {
  font-size: 0.78rem;
  min-width: 30px;
  text-align: center;
  color: var(--vp-c-text-3);
}
.simd-bit--one { font-weight: 700; }

/* ── tape output ── */
.simd-tape-out {
  padding: 0.5rem 1.25rem 0.75rem;
}
.simd-tape-out__label {
  font-size: 0.72rem;
  color: var(--vp-c-text-2);
  display: block;
  margin-bottom: 0.5rem;
}
.simd-tape-out__nodes {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
.simd-tape-node {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  padding: 0.35rem 0.65rem;
  border: 1px solid #9c27b066;
  border-radius: 6px;
  background: rgba(156,39,176,0.08);
  animation: nodeIn 0.25s ease both;
}
@keyframes nodeIn {
  from { opacity: 0; transform: scale(0.8); }
  to   { opacity: 1; transform: scale(1); }
}
.simd-tape-node__char { font-size: 0.75rem; color: #9c27b0; font-weight: 700; }
.simd-tape-node__tag  { font-size: 0.65rem; color: var(--vp-c-text-2); }

/* ── nav ── */
.simd-nav {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 1rem;
  padding: 0.85rem;
  border-top: 1px solid var(--vp-c-divider);
}
.simd-btn {
  padding: 0.35rem 0.9rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  background: var(--vp-c-bg-mute);
  color: var(--vp-c-text-1);
  font-size: 0.8rem;
  cursor: pointer;
  font-family: inherit;
  transition: border-color 0.2s, color 0.2s;
}
.simd-btn:hover:not(:disabled) { border-color: var(--vp-c-brand-1); color: var(--vp-c-brand-1); }
.simd-btn:disabled { opacity: 0.35; cursor: not-allowed; }
.simd-nav__hint { font-size: 0.78rem; color: var(--vp-c-text-2); }

/* ── transitions ── */
.fade-enter-active { transition: opacity 0.3s; }
.fade-enter-from   { opacity: 0; }
.fade-leave-active { transition: opacity 0.15s; }
.fade-leave-to     { opacity: 0; }
</style>
