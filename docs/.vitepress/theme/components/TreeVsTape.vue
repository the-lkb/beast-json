<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'

// ── Tree DOM side ──────────────────────────────────────────────────────────
interface TreeNode {
  id: number
  label: string
  children: number[]
  addr: string
  accent: string
}

const treeNodes: TreeNode[] = [
  { id: 0, label: 'Object node\nmalloc #1',   children: [1, 3], addr: '0x5a40', accent: '#f44336' },
  { id: 1, label: '"name" key\nmalloc #2',    children: [2],    addr: '0x7f23', accent: '#ff9800' },
  { id: 2, label: '"Alice" copy\nmalloc #3',  children: [],     addr: '0x3c11', accent: '#ff9800' },
  { id: 3, label: '"age" key\nmalloc #4',     children: [4],    addr: '0x9d04', accent: '#ff9800' },
  { id: 4, label: 'IntNode 30\nmalloc #5',    children: [],     addr: '0x12ef', accent: '#ff9800' },
]

// ── Tape DOM side ──────────────────────────────────────────────────────────
interface TapeCell {
  idx: number
  tag: string
  note: string
  accent: string
}

const tapeCells: TapeCell[] = [
  { idx: 0, tag: 'OBJ_START', note: 'jump→5',           accent: '#0097a7' },
  { idx: 1, tag: 'KEY',       note: '"name" → buf ptr', accent: '#00838f' },
  { idx: 2, tag: 'STRING',    note: '"Alice" → buf ptr',accent: '#4caf50' },
  { idx: 3, tag: 'KEY',       note: '"age" → buf ptr',  accent: '#00838f' },
  { idx: 4, tag: 'INT64',     note: '30 inline',        accent: '#4caf50' },
  { idx: 5, tag: 'OBJ_END',   note: 'jump→0',           accent: '#0097a7' },
]

// ── Animation state ────────────────────────────────────────────────────────
const treeVisible = ref(0)   // how many tree nodes are shown
const tapeVisible = ref(0)   // how many tape cells are shown
const phase = ref<'idle' | 'tree' | 'tape' | 'done'>('idle')

let timer: ReturnType<typeof setInterval> | null = null

function clearTimer() {
  if (timer) { clearInterval(timer); timer = null }
}

function play() {
  clearTimer()
  treeVisible.value = 0
  tapeVisible.value = 0
  phase.value = 'tree'

  // Animate tree nodes first
  timer = setInterval(() => {
    if (treeVisible.value < treeNodes.length) {
      treeVisible.value++
    } else {
      clearTimer()
      // Short pause then animate tape
      setTimeout(() => {
        phase.value = 'tape'
        timer = setInterval(() => {
          if (tapeVisible.value < tapeCells.length) {
            tapeVisible.value++
          } else {
            clearTimer()
            phase.value = 'done'
          }
        }, 220)
      }, 400)
    }
  }, 280)
}

onMounted(() => setTimeout(play, 400))
onUnmounted(() => clearTimer())
</script>

<template>
  <div class="tvt-wrap">
    <!-- Columns -->
    <div class="tvt-columns">
      <!-- ── Left: Tree DOM ── -->
      <div class="tvt-col tvt-col--tree">
        <div class="tvt-col__header tvt-col__header--bad">
          <span class="tvt-col__icon">🌲</span>
          <div>
            <div class="tvt-col__title">Tree DOM</div>
            <div class="tvt-col__sub">nlohmann / RapidJSON style</div>
          </div>
        </div>

        <div class="tvt-tree-body">
          <div
            v-for="n in treeNodes"
            :key="n.id"
            class="tvt-tree-node"
            :class="{ 'tvt-tree-node--visible': n.id < treeVisible }"
            :style="{ '--acc': n.accent, marginLeft: n.id === 0 ? '0' : n.id % 2 === 0 ? '3rem' : '1.5rem' }"
          >
            <span class="tvt-tree-node__addr">{{ n.addr }}</span>
            <span class="tvt-tree-node__label">{{ n.label }}</span>
            <!-- pointer arrow to children -->
            <span v-if="n.children.length" class="tvt-tree-node__ptr">→ ptr →</span>
          </div>

          <!-- Cache miss warning -->
          <Transition name="fade">
            <div v-if="treeVisible === treeNodes.length" class="tvt-warn">
              ⚠ 5 malloc calls · scattered heap · cache miss on every access
            </div>
          </Transition>
        </div>
      </div>

      <!-- ── Right: Tape DOM ── -->
      <div class="tvt-col tvt-col--tape">
        <div class="tvt-col__header tvt-col__header--good">
          <span class="tvt-col__icon">📼</span>
          <div>
            <div class="tvt-col__title">Lazy Tape DOM</div>
            <div class="tvt-col__sub">Beast JSON — contiguous array</div>
          </div>
        </div>

        <div class="tvt-tape-body">
          <div
            v-for="c in tapeCells"
            :key="c.idx"
            class="tvt-tape-cell"
            :class="{ 'tvt-tape-cell--visible': c.idx < tapeVisible }"
            :style="{ '--acc': c.accent }"
          >
            <span class="tvt-tape-cell__idx">tape[{{ c.idx }}]</span>
            <span class="tvt-tape-cell__tag">{{ c.tag }}</span>
            <span class="tvt-tape-cell__note">{{ c.note }}</span>
          </div>

          <!-- Win badge -->
          <Transition name="fade">
            <div v-if="tapeVisible === tapeCells.length" class="tvt-win">
              ✓ 1 malloc · sequential · zero pointer chasing
            </div>
          </Transition>
        </div>
      </div>
    </div>

    <!-- Comparison table -->
    <Transition name="fade">
      <div v-if="phase === 'done'" class="tvt-table">
        <table>
          <thead>
            <tr>
              <th>Metric</th>
              <th class="tvt-bad">Tree DOM</th>
              <th class="tvt-good">Tape DOM</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>Mallocs / parse</td>
              <td class="tvt-bad">N per element</td>
              <td class="tvt-good"><strong>1</strong></td>
            </tr>
            <tr>
              <td>Memory layout</td>
              <td class="tvt-bad">Scattered heap</td>
              <td class="tvt-good"><strong>Contiguous</strong></td>
            </tr>
            <tr>
              <td>Cache misses</td>
              <td class="tvt-bad">1–3 per access</td>
              <td class="tvt-good"><strong>~0 (sequential)</strong></td>
            </tr>
            <tr>
              <td>String storage</td>
              <td class="tvt-bad">heap copy</td>
              <td class="tvt-good"><strong>zero-copy ptr</strong></td>
            </tr>
            <tr>
              <td>Object skip</td>
              <td class="tvt-bad">O(N)</td>
              <td class="tvt-good"><strong>O(1)</strong></td>
            </tr>
          </tbody>
        </table>
      </div>
    </Transition>

    <!-- Controls -->
    <div class="tvt-controls">
      <button class="tvt-btn" @click="play">▶ Replay</button>
      <span class="tvt-hint" v-if="phase === 'tree'">Building tree…</span>
      <span class="tvt-hint" v-else-if="phase === 'tape'">Building tape…</span>
      <span class="tvt-hint" v-else-if="phase === 'done'">Comparison complete</span>
    </div>
  </div>
</template>

<style scoped>
.tvt-wrap {
  border-radius: 12px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  margin: 2rem 0;
  overflow: hidden;
  font-family: var(--vp-font-family-mono);
}

/* ── two-column layout ── */
.tvt-columns {
  display: grid;
  grid-template-columns: 1fr 1fr;
  border-bottom: 1px solid var(--vp-c-divider);
}
@media (max-width: 600px) {
  .tvt-columns { grid-template-columns: 1fr; }
}

.tvt-col {
  padding: 1.25rem 1rem;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}
.tvt-col--tree {
  border-right: 1px solid var(--vp-c-divider);
  background: rgba(244,67,54,0.03);
}
@media (max-width: 600px) {
  .tvt-col--tree { border-right: none; border-bottom: 1px solid var(--vp-c-divider); }
}
.tvt-col--tape { background: rgba(0,151,167,0.03); }

/* ── column headers ── */
.tvt-col__header {
  display: flex;
  align-items: center;
  gap: 0.6rem;
  padding-bottom: 0.6rem;
  border-bottom: 2px solid var(--vp-c-divider);
}
.tvt-col__header--bad  { border-bottom-color: rgba(244,67,54,0.5); }
.tvt-col__header--good { border-bottom-color: rgba(0,151,167,0.5); }
.tvt-col__icon  { font-size: 1.4rem; line-height: 1; }
.tvt-col__title { font-size: 0.88rem; font-weight: 700; color: var(--vp-c-text-1); }
.tvt-col__sub   { font-size: 0.68rem; color: var(--vp-c-text-3); }

/* ── tree nodes ── */
.tvt-tree-body {
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.tvt-tree-node {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 0.4rem 0.6rem;
  border: 1px solid var(--acc, #f44336);
  border-radius: 5px;
  background: color-mix(in srgb, var(--acc, #f44336) 7%, transparent);
  opacity: 0;
  transform: translateX(-12px);
  transition: opacity 0.3s, transform 0.3s;
}
.tvt-tree-node--visible { opacity: 1; transform: translateX(0); }
.tvt-tree-node__addr  { font-size: 0.62rem; color: var(--vp-c-text-3); flex-shrink: 0; }
.tvt-tree-node__label { font-size: 0.72rem; color: var(--acc, #f44336); white-space: pre-line; line-height: 1.3; flex: 1; }
.tvt-tree-node__ptr   { font-size: 0.65rem; color: var(--vp-c-text-3); flex-shrink: 0; }

.tvt-warn {
  margin-top: 0.5rem;
  padding: 0.5rem 0.75rem;
  border-radius: 6px;
  background: rgba(244,67,54,0.1);
  border: 1px solid rgba(244,67,54,0.3);
  font-size: 0.72rem;
  color: #f44336;
  font-family: var(--vp-font-family-base);
  line-height: 1.4;
}

/* ── tape cells ── */
.tvt-tape-body {
  display: flex;
  flex-direction: column;
  gap: 5px;
}
.tvt-tape-cell {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 0.4rem 0.6rem;
  border: 1px solid var(--acc, var(--vp-c-brand-1));
  border-radius: 5px;
  background: color-mix(in srgb, var(--acc, var(--vp-c-brand-1)) 8%, transparent);
  opacity: 0;
  transform: translateX(12px);
  transition: opacity 0.28s, transform 0.28s;
}
.tvt-tape-cell--visible { opacity: 1; transform: translateX(0); }
.tvt-tape-cell__idx  { font-size: 0.62rem; color: var(--vp-c-text-3); flex-shrink: 0; width: 52px; }
.tvt-tape-cell__tag  { font-size: 0.72rem; font-weight: 700; color: var(--acc, var(--vp-c-brand-1)); flex-shrink: 0; }
.tvt-tape-cell__note { font-size: 0.65rem; color: var(--vp-c-text-2); flex: 1; }

.tvt-win {
  margin-top: 0.5rem;
  padding: 0.5rem 0.75rem;
  border-radius: 6px;
  background: rgba(0,151,167,0.1);
  border: 1px solid rgba(0,151,167,0.3);
  font-size: 0.72rem;
  color: var(--vp-c-brand-1);
  font-family: var(--vp-font-family-base);
}

/* ── comparison table ── */
.tvt-table {
  padding: 1.25rem 1.25rem 0.75rem;
  overflow-x: auto;
}
.tvt-table table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.82rem;
  /* reset the global img styles that might leak */
  border: none;
  box-shadow: none;
  margin: 0;
}
.tvt-table th,
.tvt-table td {
  padding: 0.4rem 0.75rem;
  border: 1px solid var(--vp-c-divider);
  text-align: left;
}
.tvt-table th { background: var(--vp-c-bg-mute); font-size: 0.75rem; }
.tvt-bad  { color: #f44336; }
.tvt-good { color: var(--vp-c-brand-1); }

/* ── controls ── */
.tvt-controls {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 1rem;
  padding: 0.85rem;
  flex-wrap: wrap;
}
.tvt-btn {
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
.tvt-btn:hover { opacity: 0.82; }
.tvt-hint { font-size: 0.75rem; color: var(--vp-c-text-2); }

/* ── transitions ── */
.fade-enter-active { transition: opacity 0.4s ease; }
.fade-enter-from   { opacity: 0; }
</style>
