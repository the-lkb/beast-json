<template>
  <div class="ci-bench">
    <div v-if="loading" class="ci-bench-state">Loading CI benchmark data…</div>

    <template v-else-if="data && data.results && data.results.length">
      <p class="ci-bench-meta">
        Runner: <code>{{ data.runner }}</code><br/>
        Commit: <code>{{ data.commit }}</code> &nbsp;·&nbsp; {{ data.timestamp }}
      </p>

      <table>
        <thead>
          <tr>
            <th>Library</th>
            <th>Parse (μs)</th>
            <th>Serialize (μs)</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="r in data.results" :key="r.library">
            <td>{{ r.library }}</td>
            <td :class="{ best: r.parse_us === minParse }">
              {{ r.parse_us.toFixed(1) }}
            </td>
            <td :class="{ best: r.serialize_us > 0 && r.serialize_us === minSer }">
              {{ r.serialize_us > 0 ? r.serialize_us.toFixed(1) : '—' }}
            </td>
          </tr>
        </tbody>
      </table>

      <p class="ci-bench-note">
        Quick mode (15 iterations), unoptimised build (no PGO/LTO tuning). Numbers
        reflect relative ordering on shared GitHub Actions runners — not absolute
        throughput. See the tables above for PGO-optimised reference figures.
      </p>
    </template>

    <div v-else class="ci-bench-state">
      No CI benchmark data yet — data appears after the first benchmark workflow run.
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'

interface BenchResult {
  library: string
  parse_us: number
  serialize_us: number
}

interface BenchData {
  timestamp: string | null
  commit: string | null
  runner: string | null
  file: string | null
  results: BenchResult[]
}

const loading = ref(true)
const data = ref<BenchData | null>(null)

const minParse = computed(() =>
  data.value?.results.length
    ? Math.min(...data.value.results.map(r => r.parse_us))
    : Infinity
)
const minSer = computed(() =>
  data.value?.results.length
    ? Math.min(...data.value.results.filter(r => r.serialize_us > 0).map(r => r.serialize_us))
    : Infinity
)

onMounted(async () => {
  try {
    const res = await fetch('/beast-json/benchmark-results.json')
    if (res.ok) {
      const json: BenchData = await res.json()
      if (json.results?.length) data.value = json
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

.ci-bench-note {
  font-size: 0.8em;
  color: var(--vp-c-text-3);
  margin-top: 0.5rem;
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
