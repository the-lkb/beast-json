<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'

interface TapeNode {
  idx: number
  tag: string
  payload: string
  accent: string
  hasJump: boolean
  jumpTo: number | null
}

const nodes: TapeNode[] = [
  { idx: 0, tag: 'OBJ_START', payload: 'jump→5',   accent: '#0097a7', hasJump: true,  jumpTo: 5 },
  { idx: 1, tag: 'KEY',       payload: '"id"',      accent: '#00838f', hasJump: false, jumpTo: null },
  { idx: 2, tag: 'INT64',     payload: '101',       accent: '#4caf50', hasJump: false, jumpTo: null },
  { idx: 3, tag: 'KEY',       payload: '"active"',  accent: '#00838f', hasJump: false, jumpTo: null },
  { idx: 4, tag: 'BOOL_TRUE', payload: 'true',      accent: '#ff9800', hasJump: false, jumpTo: null },
  { idx: 5, tag: 'OBJ_END',   payload: 'jump→0',    accent: '#0097a7', hasJump: true,  jumpTo: 0 },
]

const visible = ref(0)
const done    = ref(false)
let timer: ReturnType<typeof setInterval> | null = null

function startPlay() {
  if (timer) return
  timer = setInterval(() => {
    if (visible.value < nodes.length) {
      visible.value++
    } else {
      done.value = true
      clearTimer()
    }
  }, 420)
}

function clearTimer() {
  if (timer) { clearInterval(timer); timer = null }
}

function replay() {
  clearTimer()
  visible.value = 0
  done.value = false
  setTimeout(startPlay, 100)
}

onMounted(() => setTimeout(startPlay, 600))
onUnmounted(() => clearTimer())
</script>

<template>
  <div class="tfd-wrap">
    <!-- JSON input -->
    <div class="tfd-input-row">
      <span class="tfd-chip tfd-chip--input">JSON Input</span>
      <code class="tfd-json">{ "id": 101, "active": true }</code>
    </div>

    <!-- Parse arrow -->
    <div class="tfd-arrow">
      <div class="tfd-arrow__line"></div>
      <span class="tfd-arrow__label">single-pass SIMD parse  ·  one malloc</span>
      <div class="tfd-arrow__head">▼</div>
    </div>

    <!-- Tape cells -->
    <div class="tfd-tape-label">
      <span class="tfd-chip tfd-chip--tape">TapeArena — contiguous memory</span>
    </div>
    <div class="tfd-cells">
      <div
        v-for="n in nodes"
        :key="n.idx"
        class="tfd-cell"
        :class="{ 'tfd-cell--visible': n.idx < visible }"
        :style="{ '--acc': n.accent }"
      >
        <span class="tfd-cell__idx">tape[{{ n.idx }}]</span>
        <span class="tfd-cell__tag">{{ n.tag }}</span>
        <span class="tfd-cell__pay">{{ n.payload }}</span>
      </div>
    </div>

    <!-- Jump arc (shown after all nodes appear) -->
    <Transition name="fade">
      <div v-if="done" class="tfd-jump-wrap">
        <svg class="tfd-jump-svg" viewBox="0 0 680 40" preserveAspectRatio="none">
          <defs>
            <marker id="arr" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
              <path d="M0,0 L0,6 L8,3 z" fill="rgba(0,229,255,0.8)"/>
            </marker>
          </defs>
          <path
            d="M 57 8 Q 340 36 623 8"
            stroke="rgba(0,229,255,0.65)"
            stroke-width="2"
            fill="none"
            stroke-dasharray="7 4"
            marker-end="url(#arr)"
          />
        </svg>
        <p class="tfd-jump-label">O(1) skip — one array read: <code>tape[tape[0].jump]</code></p>
      </div>
    </Transition>

    <!-- Controls -->
    <div class="tfd-controls">
      <button class="tfd-btn" @click="replay">▶ Replay</button>
      <span class="tfd-hint">
        {{ visible }}/{{ nodes.length }} nodes written
        <template v-if="done"> · 1 malloc total</template>
      </span>
    </div>
  </div>
</template>

<style scoped>
.tfd-wrap {
  padding: 1.5rem 1.25rem;
  border-radius: 10px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  margin: 2rem 0;
  font-family: var(--vp-font-family-mono);
  overflow: hidden;
}

/* ── chips ── */
.tfd-chip {
  display: inline-block;
  padding: 0.2rem 0.65rem;
  border-radius: 20px;
  font-size: 0.68rem;
  letter-spacing: 0.07em;
  text-transform: uppercase;
  font-weight: 700;
}
.tfd-chip--input { background: rgba(0,151,167,0.18); color: var(--vp-c-brand-1); }
.tfd-chip--tape  { background: rgba(0,151,167,0.12); color: var(--vp-c-brand-2); }

/* ── input row ── */
.tfd-input-row {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.75rem;
  flex-wrap: wrap;
  margin-bottom: 0.75rem;
}
.tfd-json {
  padding: 0.35rem 0.85rem;
  background: var(--vp-c-bg-mute);
  border-radius: 5px;
  font-size: 0.88rem;
  color: var(--vp-c-brand-1);
  white-space: nowrap;
}

/* ── parse arrow ── */
.tfd-arrow {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  margin: 0.25rem 0;
}
.tfd-arrow__line {
  width: 2px;
  height: 22px;
  background: var(--vp-c-brand-1);
  opacity: 0.5;
}
.tfd-arrow__label {
  font-size: 0.7rem;
  color: var(--vp-c-text-2);
  letter-spacing: 0.03em;
}
.tfd-arrow__head {
  color: var(--vp-c-brand-1);
  font-size: 1.1rem;
  line-height: 1;
}

/* ── tape label ── */
.tfd-tape-label {
  display: flex;
  justify-content: center;
  margin: 0.5rem 0;
}

/* ── cells ── */
.tfd-cells {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  justify-content: center;
  padding: 0.25rem 0;
}

.tfd-cell {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 3px;
  padding: 0.5rem 0.7rem;
  border: 2px solid var(--acc, var(--vp-c-brand-1));
  border-radius: 6px;
  background: color-mix(in srgb, var(--acc, var(--vp-c-brand-1)) 7%, transparent);
  min-width: 96px;
  opacity: 0;
  transform: translateY(18px) scale(0.88);
  transition: opacity 0.35s ease, transform 0.35s ease;
  transition-delay: 0s;
}
.tfd-cell--visible {
  opacity: 1;
  transform: translateY(0) scale(1);
}

.tfd-cell__idx  { font-size: 0.62rem; color: var(--vp-c-text-3); }
.tfd-cell__tag  { font-size: 0.72rem; font-weight: 700; color: var(--acc, var(--vp-c-brand-1)); }
.tfd-cell__pay  { font-size: 0.68rem; color: var(--vp-c-text-2); }

/* ── jump arc ── */
.tfd-jump-wrap {
  width: 100%;
  margin-top: 0.1rem;
}
.tfd-jump-svg {
  width: 100%;
  height: 42px;
  display: block;
}
.tfd-jump-label {
  text-align: center;
  font-size: 0.72rem;
  color: rgba(0, 229, 255, 0.9);
  margin: 0.15rem 0 0;
  font-family: var(--vp-font-family-mono);
  line-height: 1.4;
}
.tfd-jump-label code {
  background: rgba(0, 229, 255, 0.1);
  border-radius: 3px;
  padding: 0.1rem 0.3rem;
  color: rgba(0, 229, 255, 1);
  font-size: 0.72rem;
  font-family: inherit;
}

/* ── controls ── */
.tfd-controls {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 1rem;
  margin-top: 0.75rem;
  flex-wrap: wrap;
}
.tfd-btn {
  padding: 0.35rem 1rem;
  background: var(--vp-c-brand-1);
  color: #fff;
  border: none;
  border-radius: 5px;
  cursor: pointer;
  font-size: 0.82rem;
  font-family: inherit;
  transition: opacity 0.2s;
}
.tfd-btn:hover { opacity: 0.82; }
.tfd-hint {
  font-size: 0.72rem;
  color: var(--vp-c-text-2);
}

/* ── fade transition ── */
.fade-enter-active { transition: opacity 0.5s ease; }
.fade-enter-from   { opacity: 0; }

/* ── mobile ── */
@media (max-width: 600px) {
  .tfd-cell { min-width: 78px; padding: 0.4rem 0.5rem; }
  .tfd-cell__tag { font-size: 0.64rem; }
  .tfd-json { font-size: 0.78rem; }
  .tfd-jump-label { font-size: 0.68rem; }
}
</style>
