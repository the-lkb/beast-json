<script setup lang="ts">
import { ref } from 'vue'

interface ErrorScenario {
  id: string
  label: string
  category: 'parse' | 'type' | 'lifetime' | 'serialize'
  level: 'beginner' | 'intermediate' | 'expert'
  input: string       // the broken JSON (or code snippet)
  errorMark: string   // which part is bad (substring to highlight)
  stage: string       // where the error is caught
  message: string     // realistic error message
  why: string         // explanation
  fix: string         // how to fix it
  fixCode: string     // code snippet showing the fix
}

const scenarios: ErrorScenario[] = [
  // ── PARSE ERRORS ─────────────────────────────────────────────────
  {
    id: 'missing-quote',
    label: 'Unterminated string',
    category: 'parse',
    level: 'beginner',
    input: '{ "name": "Alice }',
    errorMark: '"Alice }',
    stage: 'Stage 1 (SIMD structural scan)',
    message: 'qbuem::parse_error: unterminated string literal at byte 10',
    why: 'Stage 1 scans for quote pairs. The opening quote at byte 10 has no matching closing quote before the end of input. The prefix-XOR carry algorithm detects this as an unclosed in_string_mask region.',
    fix: 'Close every string with a matching double-quote before the next structural character.',
    fixCode: '{ "name": "Alice" }   // ← closing quote added',
  },
  {
    id: 'trailing-comma',
    label: 'Trailing comma',
    category: 'parse',
    level: 'beginner',
    input: '{ "a": 1, "b": 2, }',
    errorMark: ', }',
    stage: 'Stage 2 (tape generation state machine)',
    message: 'qbuem::parse_error: unexpected token \'}\' after comma at byte 18',
    why: 'Stage 2 processes structural chars left-to-right. After a comma it expects a value or key. Seeing } means the object ended without a value — a JSON standard violation.',
    fix: 'Remove the trailing comma. JSON does not allow trailing commas (unlike JavaScript).',
    fixCode: '{ "a": 1, "b": 2 }   // ← trailing comma removed',
  },
  {
    id: 'invalid-escape',
    label: 'Invalid escape sequence',
    category: 'parse',
    level: 'intermediate',
    input: '{ "path": "C:\\\\Users\\\\q" }',
    errorMark: '\\q',
    stage: 'Stage 2 (string escape handling)',
    message: 'qbuem::parse_error: invalid escape sequence \'\\q\' at byte 22',
    why: 'Valid JSON escape sequences are: \\\\ \\\" \\/ \\b \\f \\n \\r \\t \\uXXXX.\n\\q is not a valid escape. Stage 2 detects this when processing the string value.',
    fix: 'Use valid escape sequences. For Windows paths, use \\\\\\\\ (four backslashes → two in JSON → one in the final string).',
    fixCode: '{ "path": "C:\\\\Users\\\\Alice" }',
  },
  {
    id: 'number-overflow',
    label: 'Integer overflow',
    category: 'parse',
    level: 'intermediate',
    input: '{ "big": 999999999999999999999 }',
    errorMark: '999999999999999999999',
    stage: 'Stage 2 (Russ Cox number parser)',
    message: 'qbuem::parse_error: integer overflow — value exceeds 2^56 at byte 9',
    why: 'qbuem-json\'s INT64/UINT64 payload is 56 bits (the remaining bits after the 8-bit type tag). Values > 2^56-1 cannot be stored inline. The Russ Cox algorithm detects overflow during accumulation.',
    fix: 'For very large integers, use a string representation and parse manually, or use .as<double>() accepting precision loss.',
    fixCode: '{ "big": "999999999999999999999" }  // store as string\n// or accept double precision:\nauto v = root["big"].as<double>();',
  },
  {
    id: 'unexpected-eof',
    label: 'Unexpected end of input',
    category: 'parse',
    level: 'beginner',
    input: '{ "a": { "b": 1 }',
    errorMark: '{ "b": 1 }',
    stage: 'Stage 2 (jump-patch resolver)',
    message: 'qbuem::parse_error: unexpected end of input — unclosed object at byte 7',
    why: 'Stage 2 uses a jump-patch stack. When it writes OBJ_START it pushes the tape index to be patched with the OBJ_END index later. Reaching EOF with items on the stack means the document is incomplete.',
    fix: 'Ensure every opening brace/bracket has a matching closing brace/bracket.',
    fixCode: '{ "a": { "b": 1 } }   // ← outer } added',
  },

  // ── TYPE ERRORS ───────────────────────────────────────────────────
  {
    id: 'wrong-type',
    label: 'as<T>() type mismatch',
    category: 'type',
    level: 'beginner',
    input: 'root["name"].as<int>()  // "name" is "Alice"',
    errorMark: 'as<int>()',
    stage: 'Runtime — Value::as<T>()',
    message: 'qbuem::type_error: expected INT64/UINT64, got STRING at tape[2]',
    why: 'as<T>() reads the TapeNode type tag and checks it against T. Tag 0x06 (STRING) ≠ expected tag for int (0x07 or 0x08). A type_error is thrown (or returned as an error code, depending on the API variant used).',
    fix: 'Check the type before extracting, or use the safe variant.',
    fixCode: 'if (root["name"].is_string()) {\n    auto s = root["name"].as<std::string_view>();\n}\n// or use try/catch with qbuem::type_error',
  },
  {
    id: 'null-value',
    label: 'Accessing null as non-null',
    category: 'type',
    level: 'beginner',
    input: 'root["missing"].as<int>()  // key does not exist',
    errorMark: '"missing"',
    stage: 'Runtime — Value::operator[]',
    message: 'qbuem::access_error: key "missing" not found in object',
    why: 'operator[] searches the tape for a KEY node matching the string. If not found, it returns a null Value (idx pointing to a NULL_VAL node). Calling as<int>() on null throws access_error.',
    fix: 'Check for existence before accessing, or handle the returned optional.',
    fixCode: 'if (auto v = root.find("missing"); v) {\n    auto n = v->as<int>();\n}',
  },

  // ── LIFETIME ERRORS ───────────────────────────────────────────────
  {
    id: 'dangling-view',
    label: 'Dangling string_view',
    category: 'lifetime',
    level: 'intermediate',
    input: 'std::string_view sv;\n{\n    std::string json = "{\\"a\\":\\"hello\\"}";\n    auto root = qbuem::parse(doc, json);\n    sv = root["a"].as<string_view>();\n}  // json is destroyed here\n// sv is now dangling!',
    errorMark: 'sv is now dangling',
    stage: 'Runtime — undefined behaviour (no exception)',
    message: '(no error thrown — undefined behaviour, possibly a crash or corrupt data)',
    why: 'string_view from qbuem-json points directly into the input buffer. When the input buffer (json std::string) is destroyed, the string_view\'s pointer becomes dangling. This is silent UB — AddressSanitizer will catch it.',
    fix: 'Either keep the input buffer alive as long as any string_view is in use, or copy the data out with std::string(sv).',
    fixCode: '// Option A: extend lifetime\nstd::string json = "{\\"a\\":\\"hello\\"}";\nauto root = qbuem::parse(doc, json);\nauto sv   = root["a"].as<std::string_view>();\nuse(sv);  // json still alive here ✓\n\n// Option B: copy out\nstd::string owned = std::string(root["a"].as<std::string_view>());',
  },
  {
    id: 'doc-destroyed',
    label: 'Document out of scope',
    category: 'lifetime',
    level: 'intermediate',
    input: 'qbuem::Value get_value() {\n    qbuem::Document doc;\n    auto root = qbuem::parse(doc, input);\n    return root["user"];  // doc is destroyed on return!\n}',
    errorMark: 'doc is destroyed',
    stage: 'Runtime — undefined behaviour',
    message: '(no exception — Value holds a dangling doc* pointer)',
    why: 'qbuem::Value stores a pointer to its DocumentView (doc_). When Document is destroyed, all Values referencing it become dangling. Any subsequent operation on the returned Value is UB.',
    fix: 'Never return a Value whose Document has gone out of scope. Either return the Document + Value together, or extract the data into owned types before returning.',
    fixCode: '// Return owned data instead:\nstd::string get_name() {\n    qbuem::Document doc;\n    auto root = qbuem::parse(doc, input);\n    return std::string(root["user"]["name"].as<std::string_view>());\n}',
  },

  // ── SERIALIZE ERRORS ─────────────────────────────────────────────
  {
    id: 'buffer-too-small',
    label: 'Output buffer overflow',
    category: 'serialize',
    level: 'intermediate',
    input: 'char buf[16];\nqbuem::write_to(buf, 16, value);  // value is large',
    errorMark: 'buf[16]',
    stage: 'Runtime — Serializer::write_to()',
    message: 'qbuem::serialize_error: output buffer capacity exceeded',
    why: 'The stream-push serializer writes directly into the provided buffer. If the output JSON is larger than the buffer size, write_to() returns an error rather than overflowing (no buffer overflow vulnerability).',
    fix: 'Use a pre-sized std::string (with reserve) so the serializer can grow it automatically.',
    fixCode: 'std::string buf;\nbuf.reserve(4096);          // warm up\nqbuem::write_to(buf, value); // auto-grows if needed',
  },
]

type Category = 'all' | 'parse' | 'type' | 'lifetime' | 'serialize'
const filterCat = ref<Category>('all')
const activeId  = ref<string | null>(null)

const filtered = computed(() =>
  scenarios.filter(s => filterCat.value === 'all' || s.category === filterCat.value)
)

const activeScenario = computed(() =>
  scenarios.find(s => s.id === activeId.value) ?? null
)

function toggle(id: string) {
  activeId.value = activeId.value === id ? null : id
}

const catLabels: Record<Category, string> = {
  all: 'All', parse: 'Parse Errors', type: 'Type Errors', lifetime: 'Lifetime', serialize: 'Serialize',
}
const catColors: Record<string, string> = {
  parse: '#f44336', type: '#ff9800', lifetime: '#9c27b0', serialize: '#2196f3',
}
const levelLabels: Record<string, string> = {
  beginner: '🟢 Beginner', intermediate: '🟡 Intermediate', expert: '🔴 Expert',
}
</script>

<template>
  <div class="pem-wrap">
    <!-- Category filter -->
    <div class="pem-filters">
      <button
        v-for="(label, key) in catLabels"
        :key="key"
        class="pem-filter-btn"
        :class="{ active: filterCat === key }"
        :style="key !== 'all' && filterCat === key ? { borderBottomColor: catColors[key], color: catColors[key] } : {}"
        @click="filterCat = key as Category; activeId = null"
      >{{ label }}</button>
    </div>

    <!-- Scenario list -->
    <div class="pem-list">
      <button
        v-for="s in filtered"
        :key="s.id"
        class="pem-item"
        :class="{ 'pem-item--open': activeId === s.id }"
        :style="{ '--cat': catColors[s.category] }"
        @click="toggle(s.id)"
      >
        <div class="pem-item__header">
          <span class="pem-item__cat-dot" :style="{ background: catColors[s.category] }"></span>
          <span class="pem-item__label">{{ s.label }}</span>
          <span class="pem-item__level">{{ levelLabels[s.level] }}</span>
          <span class="pem-item__arrow">{{ activeId === s.id ? '▲' : '▼' }}</span>
        </div>

        <!-- Broken code -->
        <div v-if="activeId === s.id" class="pem-item__body" @click.stop>
          <div class="pem-section">
            <span class="pem-section__label">Broken input:</span>
            <pre class="pem-code pem-code--bad">{{ s.input }}</pre>
          </div>

          <div class="pem-section">
            <span class="pem-section__label">Detected at:</span>
            <code class="pem-badge">{{ s.stage }}</code>
          </div>

          <div class="pem-section">
            <span class="pem-section__label">Error message:</span>
            <pre class="pem-code pem-code--error">{{ s.message }}</pre>
          </div>

          <div class="pem-section">
            <span class="pem-section__label">Why this happens:</span>
            <p class="pem-text">{{ s.why }}</p>
          </div>

          <div class="pem-section">
            <span class="pem-section__label">How to fix:</span>
            <p class="pem-text">{{ s.fix }}</p>
            <pre class="pem-code pem-code--good">{{ s.fixCode }}</pre>
          </div>
        </div>
      </button>
    </div>
  </div>
</template>

<style scoped>
.pem-wrap {
  border-radius: 12px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  margin: 2rem 0;
  overflow: hidden;
  font-family: var(--vp-font-family-base);
}

/* ── filters ── */
.pem-filters {
  display: flex;
  border-bottom: 1px solid var(--vp-c-divider);
  flex-wrap: wrap;
  gap: 0;
}
.pem-filter-btn {
  flex: 1;
  padding: 0.6rem 0.4rem;
  background: none;
  border: none;
  border-bottom: 3px solid transparent;
  cursor: pointer;
  font-size: 0.75rem;
  font-family: inherit;
  color: var(--vp-c-text-2);
  transition: all 0.2s;
  white-space: nowrap;
  min-width: 60px;
}
.pem-filter-btn:hover { background: var(--vp-c-bg-mute); color: var(--vp-c-text-1); }
.pem-filter-btn.active { font-weight: 700; border-bottom-color: var(--vp-c-brand-1); color: var(--vp-c-brand-1); }

/* ── list ── */
.pem-list {
  display: flex;
  flex-direction: column;
}

/* ── item ── */
.pem-item {
  border: none;
  border-bottom: 1px solid var(--vp-c-divider);
  background: none;
  cursor: pointer;
  text-align: left;
  font-family: inherit;
  padding: 0;
  transition: background 0.15s;
}
.pem-item:last-child { border-bottom: none; }
.pem-item:hover { background: var(--vp-c-bg-mute); }
.pem-item--open { background: var(--vp-c-bg-mute); }

.pem-item__header {
  display: flex;
  align-items: center;
  gap: 0.6rem;
  padding: 0.75rem 1rem;
  flex-wrap: wrap;
}
.pem-item__cat-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  flex-shrink: 0;
}
.pem-item__label {
  font-size: 0.88rem;
  font-weight: 600;
  color: var(--vp-c-text-1);
  flex: 1;
}
.pem-item__level {
  font-size: 0.72rem;
  color: var(--vp-c-text-2);
  flex-shrink: 0;
}
.pem-item__arrow {
  font-size: 0.65rem;
  color: var(--vp-c-text-3);
  flex-shrink: 0;
}

/* ── body ── */
.pem-item__body {
  padding: 0.25rem 1rem 1rem;
  display: flex;
  flex-direction: column;
  gap: 0.85rem;
  cursor: auto;
}

.pem-section { display: flex; flex-direction: column; gap: 0.3rem; }
.pem-section__label {
  font-size: 0.68rem;
  text-transform: uppercase;
  letter-spacing: 0.07em;
  font-weight: 700;
  color: var(--vp-c-brand-1);
}

.pem-code {
  margin: 0;
  padding: 0.6rem 0.85rem;
  border-radius: 6px;
  font-family: var(--vp-font-family-mono);
  font-size: 0.8rem;
  line-height: 1.5;
  white-space: pre-wrap;
  word-break: break-all;
  overflow-x: auto;
}
.pem-code--bad   { background: rgba(244,67,54,0.08);  border-left: 3px solid #f44336; color: var(--vp-c-text-1); }
.pem-code--error { background: rgba(244,67,54,0.06);  border-left: 3px solid #ff5722; color: #f44336; }
.pem-code--good  { background: rgba(76,175,80,0.08);  border-left: 3px solid #4caf50; color: var(--vp-c-text-1); }

.pem-badge {
  font-family: var(--vp-font-family-mono);
  font-size: 0.78rem;
  padding: 0.15rem 0.55rem;
  background: rgba(0,151,167,0.12);
  color: var(--vp-c-brand-1);
  border-radius: 4px;
  width: fit-content;
}

.pem-text {
  margin: 0;
  font-size: 0.84rem;
  color: var(--vp-c-text-1);
  line-height: 1.6;
  white-space: pre-line;
}

/* ── mobile ── */
@media (max-width: 500px) {
  .pem-item__header { padding: 0.6rem 0.75rem; }
  .pem-item__body { padding: 0.25rem 0.75rem 0.75rem; }
  .pem-item__level { display: none; }
}
</style>
