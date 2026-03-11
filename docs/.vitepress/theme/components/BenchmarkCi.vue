<template>
  <div class="ci-bench">
    <div v-if="loading" class="ci-bench-state">Loading CI benchmark data…</div>

    <template v-else-if="data && data.platforms && data.platforms.length">
      <p class="ci-bench-meta">
        Commit: <code>{{ data.commit }}</code> &nbsp;·&nbsp; {{ data.timestamp }}
        &nbsp;·&nbsp; file: <code>{{ data.file }}</code>
      </p>

      <template v-for="p in data.platforms" :key="p.arch">
        <h4 class="ci-bench-platform">{{ p.label }}</h4>
        <table v-if="p.results && p.results.length">
          <thead>
            <tr>
              <th>Library</th>
              <th>Parse (μs)</th>
              <th>Serialize (μs)</th>
              <th>Alloc (KB)</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="r in p.results" :key="r.library">
              <td>{{ r.library }}</td>
              <td :class="{ best: r.parse_us === minParse(p.results) }">
                {{ r.parse_us.toFixed(1) }}
              </td>
              <td :class="{ best: r.serialize_us > 0 && r.serialize_us === minSer(p.results) }">
                {{ r.serialize_us > 0 ? r.serialize_us.toFixed(1) : '—' }}
              </td>
              <td :class="{ best: r.alloc_kb > 0 && r.alloc_kb === minAlloc(p.results) }">
                {{ r.alloc_kb > 0 ? r.alloc_kb : '—' }}
              </td>
            </tr>
          </tbody>
        </table>
        <p v-else class="ci-bench-state">No data for this platform.</p>
      </template>

      <p class="ci-bench-note">
        Quick mode (15 iterations), Release build without PGO/LTO.
        Numbers reflect relative ordering on shared GitHub Actions runners —
        not absolute throughput.
      </p>
    </template>

    <div v-else class="ci-bench-state">
      No CI benchmark data yet — data appears after the first benchmark workflow run.
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'

interface BenchResult {
  library: string
  parse_us: number
  serialize_us: number
  alloc_kb: number
}

interface PlatformData {
  arch: string
  label: string
  results: BenchResult[]
}

interface BenchData {
  timestamp: string
  commit: string
  file: string
  platforms: PlatformData[]
}

const loading = ref(true)
const data = ref<BenchData | null>(null)

function minParse(results: BenchResult[]): number {
  return results.length ? Math.min(...results.map(r => r.parse_us)) : Infinity
}

function minSer(results: BenchResult[]): number {
  const vals = results.filter(r => r.serialize_us > 0).map(r => r.serialize_us)
  return vals.length ? Math.min(...vals) : Infinity
}

function minAlloc(results: BenchResult[]): number {
  const vals = results.filter(r => r.alloc_kb > 0).map(r => r.alloc_kb)
  return vals.length ? Math.min(...vals) : Infinity
}

onMounted(async () => {
  try {
    const res = await fetch('/beast-json/benchmark-results.json')
    if (res.ok) {
      const json: BenchData = await res.json()
      if (json.platforms?.length) data.value = json
    }
  } catch {
    // network or parse error — show "no data" state
  } finally {
    loading.value = false
  }
})
</script>

<style scoped>
.ci-bench { margin: 1.5rem 0; }

.ci-bench-meta {
  font-size: 0.85em;
  color: var(--vp-c-text-2);
  margin-bottom: 0.75rem;
}

.ci-bench-platform {
  margin-top: 1.25rem;
  margin-bottom: 0.4rem;
  font-size: 1em;
  font-weight: 600;
}

.ci-bench-note {
  font-size: 0.8em;
  color: var(--vp-c-text-3);
  margin-top: 0.75rem;
}

.ci-bench-state {
  color: var(--vp-c-text-2);
  font-style: italic;
}

td.best {
  font-weight: 700;
  color: var(--vp-c-brand-1);
}
</style>
