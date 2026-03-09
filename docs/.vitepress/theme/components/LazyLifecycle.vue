<script setup lang="ts">
import { ref } from 'vue'

interface Phase {
  num: number
  title: string
  sub: string
  accent: string
  icon: string
  steps: string[]
  output: string
  outputLabel: string
}

const phases: Phase[] = [
  {
    num: 1,
    title: 'PARSE',
    sub: 'eager · once',
    accent: '#0097a7',
    icon: '⚡',
    steps: [
      'SIMD Stage 1: load 64 bytes/cycle into zmm0',
      'VPCMPEQB ×8 → structural_mask bitmask',
      'Prefix-XOR quote masking via PCLMULQDQ',
      'Stage 2: walk bitset, write TapeNodes',
      'One malloc → TapeArena filled',
    ],
    output: 'DocumentView',
    outputLabel: 'TapeArena ready for query',
  },
  {
    num: 2,
    title: 'NAVIGATE',
    sub: 'zero cost · no allocation',
    accent: '#00838f',
    icon: '🔍',
    steps: [
      'root["user"]  →  walk tape KEY nodes',
      'OBJ_START jump index → O(1) skip whole subtrees',
      'Returns Value{ doc*, idx=2 }',
      'No data extracted — only a 16-byte handle',
      'Repeat navigation stays fully cache-hot',
    ],
    output: 'Value{ doc*, idx }',
    outputLabel: '16-byte handle — no heap, no copy',
  },
  {
    num: 3,
    title: 'EXTRACT',
    sub: 'lazy · on demand',
    accent: '#4caf50',
    icon: '📦',
    steps: [
      '.as<string_view>()  →  reads tape[idx].offset',
      'Returns string_view directly into input buffer',
      '.as<int>()  →  reads tape[idx].meta inline',
      'One array read per field — zero allocation',
      'Unread fields cost nothing',
    ],
    output: 'Typed C++ value',
    outputLabel: 'string_view / int64 / double — zero copy',
  },
]

const active = ref(0)

const nums = ['①', '②', '③']
</script>

<template>
  <div class="llc-wrap">
    <!-- Phase tab bar -->
    <div class="llc-tabs" role="tablist">
      <button
        v-for="(p, i) in phases"
        :key="i"
        class="llc-tab"
        :class="{ 'llc-tab--active': active === i }"
        :style="{ '--acc': p.accent }"
        role="tab"
        :aria-selected="active === i"
        @click="active = i"
      >
        <span class="llc-tab__num">{{ nums[i] }}</span>
        <span class="llc-tab__title">{{ p.title }}</span>
        <span class="llc-tab__sub">{{ p.sub }}</span>
      </button>
    </div>

    <!-- Flow progress bar -->
    <div class="llc-progress" aria-hidden="true">
      <div
        v-for="(p, i) in phases"
        :key="i"
        class="llc-progress__item"
      >
        <div
          class="llc-progress__dot"
          :class="{ 'llc-progress__dot--done': i <= active }"
          :style="i <= active ? { background: p.accent, boxShadow: `0 0 10px ${p.accent}88` } : {}"
        >
          <span v-if="i < active">✓</span>
          <span v-else-if="i === active">{{ nums[i] }}</span>
          <span v-else>{{ nums[i] }}</span>
        </div>
        <div
          v-if="i < phases.length - 1"
          class="llc-progress__line"
          :class="{ 'llc-progress__line--done': i < active }"
          :style="i < active ? { background: phases[i].accent } : {}"
        ></div>
      </div>
    </div>

    <!-- Active phase panel -->
    <Transition name="llc-slide" mode="out-in">
      <div
        :key="active"
        class="llc-panel"
        :style="{ '--acc': phases[active].accent, '--acc-soft': phases[active].accent + '18' }"
      >
        <div class="llc-panel__header">
          <span class="llc-panel__icon">{{ phases[active].icon }}</span>
          <div>
            <h3 class="llc-panel__title">Phase {{ phases[active].num }}: {{ phases[active].title }}</h3>
            <p class="llc-panel__sub">{{ phases[active].sub }}</p>
          </div>
        </div>

        <ul class="llc-steps">
          <li
            v-for="(step, si) in phases[active].steps"
            :key="si"
            class="llc-step"
            :style="{ animationDelay: `${si * 80}ms` }"
          >
            <span class="llc-step__bullet">→</span>
            <span class="llc-step__text">{{ step }}</span>
          </li>
        </ul>

        <div class="llc-output">
          <span class="llc-output__label">Output</span>
          <code class="llc-output__code">{{ phases[active].output }}</code>
          <span class="llc-output__note">{{ phases[active].outputLabel }}</span>
        </div>
      </div>
    </Transition>

    <!-- Prev / Next buttons -->
    <div class="llc-nav">
      <button
        class="llc-nav__btn"
        :disabled="active === 0"
        @click="active = Math.max(0, active - 1)"
      >← Prev</button>
      <span class="llc-nav__hint">{{ active + 1 }} / {{ phases.length }}</span>
      <button
        class="llc-nav__btn"
        :disabled="active === phases.length - 1"
        @click="active = Math.min(phases.length - 1, active + 1)"
      >Next →</button>
    </div>
  </div>
</template>

<style scoped>
/* ── wrapper ── */
.llc-wrap {
  border-radius: 12px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  margin: 2rem 0;
  overflow: hidden;
  font-family: var(--vp-font-family-base);
}

/* ── tab bar ── */
.llc-tabs {
  display: flex;
  border-bottom: 1px solid var(--vp-c-divider);
}
.llc-tab {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  padding: 0.85rem 0.5rem;
  background: none;
  border: none;
  border-bottom: 3px solid transparent;
  cursor: pointer;
  transition: background 0.2s, border-color 0.2s;
  font-family: inherit;
}
.llc-tab:hover { background: var(--vp-c-bg-mute); }
.llc-tab--active {
  border-bottom-color: var(--acc);
  background: color-mix(in srgb, var(--acc) 6%, transparent);
}
.llc-tab__num  { font-size: 1.1rem; line-height: 1; }
.llc-tab__title {
  font-size: 0.8rem;
  font-weight: 700;
  color: var(--vp-c-text-1);
  letter-spacing: 0.05em;
}
.llc-tab--active .llc-tab__title { color: var(--acc); }
.llc-tab__sub  { font-size: 0.62rem; color: var(--vp-c-text-3); }

/* ── progress bar ── */
.llc-progress {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1rem 1.5rem 0;
  gap: 0;
}
.llc-progress__item {
  display: flex;
  align-items: center;
  flex: 1;
}
.llc-progress__item:last-child { flex: 0; }

.llc-progress__dot {
  width: 30px;
  height: 30px;
  border-radius: 50%;
  border: 2px solid var(--vp-c-divider);
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 0.8rem;
  font-weight: 700;
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-2);
  transition: all 0.3s ease;
  flex-shrink: 0;
  z-index: 1;
}
.llc-progress__dot--done {
  color: #fff;
  border-color: transparent;
}
.llc-progress__line {
  flex: 1;
  height: 2px;
  background: var(--vp-c-divider);
  transition: background 0.4s ease;
}
.llc-progress__line--done {
  /* color set inline */
}

/* ── panel ── */
.llc-panel {
  padding: 1.5rem;
}
.llc-panel__header {
  display: flex;
  align-items: flex-start;
  gap: 0.75rem;
  margin-bottom: 1.25rem;
}
.llc-panel__icon {
  font-size: 2rem;
  line-height: 1;
  flex-shrink: 0;
}
.llc-panel__title {
  margin: 0;
  font-size: 1.1rem;
  font-weight: 700;
  color: var(--acc);
}
.llc-panel__sub {
  margin: 0.1rem 0 0;
  font-size: 0.78rem;
  color: var(--vp-c-text-2);
}

/* ── steps ── */
.llc-steps {
  list-style: none;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}
.llc-step {
  display: flex;
  gap: 0.5rem;
  font-size: 0.88rem;
  font-family: var(--vp-font-family-mono);
  color: var(--vp-c-text-1);
  line-height: 1.5;
  animation: stepIn 0.28s ease both;
}
@keyframes stepIn {
  from { opacity: 0; transform: translateX(-10px); }
  to   { opacity: 1; transform: translateX(0); }
}
.llc-step__bullet {
  color: var(--acc);
  font-weight: 700;
  flex-shrink: 0;
  margin-top: 1px;
}

/* ── output box ── */
.llc-output {
  margin-top: 1.25rem;
  padding: 0.85rem 1rem;
  border-radius: 8px;
  border-left: 3px solid var(--acc);
  background: var(--acc-soft);
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.5rem 0.75rem;
}
.llc-output__label {
  font-size: 0.7rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--acc);
}
.llc-output__code {
  font-family: var(--vp-font-family-mono);
  font-size: 0.88rem;
  color: var(--vp-c-text-1);
  font-weight: 600;
}
.llc-output__note {
  font-size: 0.78rem;
  color: var(--vp-c-text-2);
  width: 100%;
  margin-top: 0.1rem;
}

/* ── nav ── */
.llc-nav {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 1rem;
  padding: 1rem;
  border-top: 1px solid var(--vp-c-divider);
}
.llc-nav__btn {
  padding: 0.38rem 1rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  background: var(--vp-c-bg-mute);
  color: var(--vp-c-text-1);
  font-size: 0.82rem;
  cursor: pointer;
  font-family: inherit;
  transition: background 0.2s, border-color 0.2s;
}
.llc-nav__btn:hover:not(:disabled) {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
}
.llc-nav__btn:disabled {
  opacity: 0.35;
  cursor: not-allowed;
}
.llc-nav__hint {
  font-size: 0.78rem;
  color: var(--vp-c-text-2);
}

/* ── transitions ── */
.llc-slide-enter-active,
.llc-slide-leave-active { transition: all 0.22s ease; }
.llc-slide-enter-from   { opacity: 0; transform: translateX(20px); }
.llc-slide-leave-to     { opacity: 0; transform: translateX(-20px); }

/* ── mobile ── */
@media (max-width: 540px) {
  .llc-tab__title { font-size: 0.72rem; }
  .llc-tab__sub   { display: none; }
  .llc-panel      { padding: 1rem; }
  .llc-step       { font-size: 0.8rem; }
}
</style>
