<template>
  <div class="bench-wrap">
    <div v-if="loading" class="bench-state">Loading benchmark data…</div>

    <template v-else-if="data">
      <!-- Meta -->
      <div class="bench-meta">
        <span>Commit <code>{{ data.commit }}</code></span>
        <span class="sep">·</span>
        <span>{{ formattedDate }}</span>
      </div>

      <!-- Architecture tabs -->
      <div class="arch-tabs">
        <button
          v-for="p in data.platforms"
          :key="p.arch"
          :class="['arch-tab', { active: selectedArch === p.arch }]"
          @click="selectedArch = p.arch"
        >
          <span class="arch-icon">{{ archIcon(p.arch) }}</span>
          {{ archLabel(p.arch) }}
        </button>
      </div>

      <!-- Section tabs -->
      <div class="section-tabs">
        <button
          v-for="s in SECTIONS"
          :key="s.key"
          :class="['section-tab', { active: selectedSection === s.key }]"
          @click="selectedSection = s.key"
        >
          <span class="section-icon">{{ s.icon }}</span>
          {{ s.label }}
        </button>
      </div>

      <!-- Dataset selector -->
      <div v-if="availableFiles.length > 1" class="file-tabs">
        <span class="row-label">Dataset:</span>
        <button
          v-for="f in availableFiles"
          :key="f"
          :class="['file-tab', { active: selectedFile === f }]"
          @click="selectedFile = f"
        >{{ f }}</button>
      </div>

      <!-- Metric tabs -->
      <div class="metric-tabs">
        <button
          v-for="m in METRICS"
          :key="m.key"
          :class="['metric-tab', { active: selectedMetric === m.key }]"
          @click="selectedMetric = m.key"
        >{{ m.label }}</button>
        <span class="metric-hint">↓ lower is better</span>
      </div>

      <!-- Bar chart -->
      <div v-if="sortedResults.length" class="chart">
        <div v-for="(r, i) in sortedResults" :key="r.library" class="bar-row">
          <div class="rank" :class="{ gold: i === 0 }">{{ i + 1 }}</div>
          <div class="lib" :class="{ qbuem: r.isQbuem }">{{ r.library }}</div>
          <div class="track">
            <div
              class="fill"
              :class="{ qbuem: r.isQbuem, na: r.value <= 0 }"
              :style="{ width: r.value > 0 ? pct(r.value) : '0%' }"
            />
          </div>
          <div class="val" :class="{ qbuem: r.isQbuem }">
            {{ r.value > 0 ? fmtVal(r.value) : '—' }}
          </div>
        </div>
      </div>
      <div v-else class="bench-state bench-no-section">
        No results for this platform/section yet —
        data populates automatically after the next CI benchmark run.
      </div>

      <p class="bench-note">
        Quick mode · Release build · no PGO/LTO ·
        Relative ordering on shared GitHub Actions runners.
      </p>
    </template>

    <div v-else class="bench-state">
      No CI benchmark data yet — appears after the first benchmark workflow run.
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, watch, onMounted } from 'vue'

interface BenchResult {
  library: string
  parse_us: number
  serialize_us: number
  alloc_kb: number
}

interface FileData {
  file: string
  results: BenchResult[]
}

interface PlatformData {
  arch: string
  label: string
  // New two-section schema
  general?: FileData[]
  structs?: FileData[]
  // Legacy flat format (backward compat)
  files?: FileData[]
  results?: BenchResult[]
}

interface BenchData {
  timestamp: string
  commit: string
  platforms: PlatformData[]
}

const SECTIONS = [
  { key: 'general', label: 'General JSON',    icon: '📄' },
  { key: 'structs', label: 'Struct Binding',  icon: '🧱' },
] as const
type SectionKey = typeof SECTIONS[number]['key']

const METRICS = [
  { key: 'parse_us',     label: 'Parse (μs)'    },
  { key: 'serialize_us', label: 'Serialize (μs)' },
  { key: 'alloc_kb',     label: 'Alloc (KB)'     },
] as const
type MetricKey = typeof METRICS[number]['key']

const ARCH_LABEL: Record<string, string> = {
  'x86_64':        'x86_64',
  'apple-aarch64': 'Apple Silicon',
  'linux-aarch64': 'Linux aarch64',
}

const ARCH_ICON: Record<string, string> = {
  'x86_64':        '🖥',
  'apple-aarch64': '',
  'linux-aarch64': '🐧',
}

const loading         = ref(true)
const data            = ref<BenchData | null>(null)
const selectedArch    = ref('')
const selectedSection = ref<SectionKey>('general')
const selectedFile    = ref('')
const selectedMetric  = ref<MetricKey>('parse_us')

const formattedDate = computed(() => {
  if (!data.value?.timestamp) return ''
  return new Date(data.value.timestamp).toLocaleString('en-US', {
    year: 'numeric', month: 'short', day: 'numeric',
    hour: '2-digit', minute: '2-digit', timeZone: 'UTC', timeZoneName: 'short',
  })
})

const currentPlatform = computed(() =>
  data.value?.platforms.find(p => p.arch === selectedArch.value) ?? null
)

// Items for the active section on the current platform
const sectionItems = computed((): FileData[] => {
  const p = currentPlatform.value
  if (!p) return []

  // New schema: p.general / p.structs
  if (selectedSection.value === 'general') {
    if (p.general?.length) return p.general
    // Legacy: p.files contains everything; filter by known file patterns
    if (p.files?.length) {
      const jsonFiles = new Set(['twitter.json','canada.json','citm_catalog.json','gsoc-2018.json','harsh.json'])
      return p.files.filter(f => jsonFiles.has(f.file) || f.file.endsWith('.json'))
    }
  }
  if (selectedSection.value === 'structs') {
    if (p.structs?.length) return p.structs
    // Legacy: p.files — everything that isn't a .json file
    if (p.files?.length) {
      return p.files.filter(f => !f.file.endsWith('.json'))
    }
  }
  return []
})

const availableFiles = computed(() => sectionItems.value.map(f => f.file))

// Reset selectedFile when section or platform changes
watch([currentPlatform, selectedSection], () => {
  const first = availableFiles.value[0] ?? ''
  if (!availableFiles.value.includes(selectedFile.value)) {
    selectedFile.value = first
  }
}, { immediate: true })

const currentResults = computed(() => {
  const fd = sectionItems.value.find(f => f.file === selectedFile.value) ?? sectionItems.value[0]
  return fd?.results ?? []
})

const sortedResults = computed(() => {
  const key = selectedMetric.value
  return [...currentResults.value]
    .map(r => ({
      ...r,
      value: r[key] as number,
      isQbuem: r.library.toLowerCase().includes('qbuem'),
    }))
    .sort((a, b) => {
      if (a.value <= 0) return 1
      if (b.value <= 0) return -1
      return a.value - b.value
    })
})

const maxValue = computed(() => {
  const vals = sortedResults.value.filter(r => r.value > 0).map(r => r.value)
  return vals.length ? Math.max(...vals) : 1
})

function pct(v: number): string {
  return (v / maxValue.value * 100).toFixed(1) + '%'
}

function fmtVal(v: number): string {
  if (selectedMetric.value === 'alloc_kb') return v.toLocaleString() + ' KB'
  // Show ns for sub-microsecond values (typical in HFT struct section)
  if (selectedMetric.value !== 'alloc_kb' && v < 1) return (v * 1000).toFixed(0) + ' ns'
  return v.toFixed(2) + ' μs'
}

function archLabel(arch: string): string { return ARCH_LABEL[arch] ?? arch }
function archIcon(arch: string):  string { return ARCH_ICON[arch]  ?? '💻' }

onMounted(async () => {
  try {
    const res = await fetch('/qbuem-json/benchmark-results.json')
    if (res.ok) {
      const json: BenchData = await res.json()
      if (json.platforms?.length) {
        data.value = json
        selectedArch.value = json.platforms[0].arch
      }
    }
  } catch { /* show "no data" state */ }
  finally  { loading.value = false }
})
</script>

<style scoped>
/* ── Wrapper ──────────────────────────────────────────────────────────── */
.bench-wrap { margin: 1.5rem 0; }

/* ── Meta line ───────────────────────────────────────────────────────── */
.bench-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 0.25rem 0.5rem;
  font-size: 0.82em;
  color: var(--vp-c-text-2);
  margin-bottom: 1rem;
}
.bench-meta .sep { color: var(--vp-c-divider); }

/* ── Architecture tabs ───────────────────────────────────────────────── */
.arch-tabs {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  margin-bottom: 0.75rem;
}

.arch-tab {
  display: flex;
  align-items: center;
  gap: 0.35rem;
  padding: 0.45rem 1rem;
  border: 1.5px solid var(--vp-c-divider);
  border-radius: 9999px;
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-2);
  font-size: 0.85em;
  font-weight: 500;
  cursor: pointer;
  transition: border-color 0.15s, color 0.15s, background 0.15s;
}
.arch-tab:hover { border-color: var(--vp-c-brand-2); color: var(--vp-c-text-1); }
.arch-tab.active {
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
  color: var(--vp-c-brand-1);
  font-weight: 600;
}

/* ── Section tabs ────────────────────────────────────────────────────── */
.section-tabs {
  display: flex;
  gap: 0.4rem;
  margin-bottom: 0.75rem;
}

.section-tab {
  display: flex;
  align-items: center;
  gap: 0.4rem;
  padding: 0.5rem 1.25rem;
  border: 1.5px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-2);
  font-size: 0.88em;
  font-weight: 500;
  cursor: pointer;
  transition: border-color 0.15s, color 0.15s, background 0.15s;
}
.section-tab:hover { border-color: var(--vp-c-brand-2); color: var(--vp-c-text-1); }
.section-tab.active {
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
  color: var(--vp-c-brand-1);
  font-weight: 700;
}
.section-icon { font-size: 1em; }

/* ── Dataset tabs ────────────────────────────────────────────────────── */
.file-tabs {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 0.35rem;
  margin-bottom: 0.6rem;
}

.row-label {
  font-size: 0.78em;
  color: var(--vp-c-text-3);
  margin-right: 0.2rem;
}

.file-tab {
  padding: 0.2rem 0.6rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 4px;
  background: transparent;
  color: var(--vp-c-text-2);
  font-size: 0.78em;
  font-family: var(--vp-font-family-mono);
  cursor: pointer;
  transition: border-color 0.15s, color 0.15s, background 0.15s;
}
.file-tab:hover { color: var(--vp-c-text-1); }
.file-tab.active {
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
  color: var(--vp-c-brand-1);
  font-weight: 600;
}

/* ── Metric tabs ─────────────────────────────────────────────────────── */
.metric-tabs {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 0.35rem;
  margin-bottom: 1.25rem;
}

.metric-tab {
  padding: 0.25rem 0.75rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 4px;
  background: transparent;
  color: var(--vp-c-text-2);
  font-size: 0.8em;
  font-weight: 500;
  cursor: pointer;
  transition: border-color 0.15s, color 0.15s, background 0.15s;
}
.metric-tab:hover { color: var(--vp-c-text-1); }
.metric-tab.active {
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
  color: var(--vp-c-brand-1);
  font-weight: 600;
}
.metric-hint {
  margin-left: auto;
  font-size: 0.78em;
  color: var(--vp-c-text-3);
  font-style: italic;
}

/* ── Bar chart ───────────────────────────────────────────────────────── */
.chart {
  display: flex;
  flex-direction: column;
  gap: 0.55rem;
}

.bar-row {
  display: grid;
  grid-template-columns: 1.4rem 150px 1fr 90px;
  align-items: center;
  gap: 0.6rem;
}

.rank {
  font-size: 0.72em;
  font-weight: 700;
  color: var(--vp-c-text-3);
  text-align: right;
  line-height: 1;
}
.rank.gold { color: #f5a623; }

.lib {
  font-size: 0.82em;
  font-family: var(--vp-font-family-mono);
  color: var(--vp-c-text-2);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.lib.qbuem { color: var(--vp-c-brand-1); font-weight: 700; }

.track {
  position: relative;
  height: 22px;
  background: var(--vp-c-bg-soft);
  border-radius: 4px;
  overflow: hidden;
}

.fill {
  height: 100%;
  border-radius: 4px;
  background: var(--vp-c-gray-2, #ccc);
  transition: width 0.45s cubic-bezier(0.4, 0, 0.2, 1);
}
.fill.qbuem { background: var(--vp-c-brand-1); opacity: 0.85; }
.fill.na { background: transparent; }

.val {
  font-size: 0.78em;
  font-family: var(--vp-font-family-mono);
  color: var(--vp-c-text-2);
  text-align: right;
  white-space: nowrap;
}
.val.qbuem { color: var(--vp-c-brand-1); font-weight: 700; }

/* ── Misc ────────────────────────────────────────────────────────────── */
.bench-note {
  font-size: 0.77em;
  color: var(--vp-c-text-3);
  margin-top: 1rem;
}

.bench-state {
  font-style: italic;
  color: var(--vp-c-text-2);
}
.bench-no-section {
  margin-top: 0.5rem;
  padding: 0.75rem 1rem;
  background: var(--vp-c-bg-soft);
  border-radius: 6px;
  font-size: 0.88em;
}

/* ── Responsive ──────────────────────────────────────────────────────── */
@media (max-width: 560px) {
  .bar-row { grid-template-columns: 1.4rem 100px 1fr 75px; gap: 0.4rem; }
  .metric-hint { display: none; }
  .section-tab { padding: 0.4rem 0.8rem; font-size: 0.82em; }
}
</style>
