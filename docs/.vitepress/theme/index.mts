import DefaultTheme from 'vitepress/theme'
import './custom.css'
import TapeFlowDiagram from './components/TapeFlowDiagram.vue'
import LazyLifecycle from './components/LazyLifecycle.vue'
import SimdPipeline from './components/SimdPipeline.vue'
import TreeVsTape from './components/TreeVsTape.vue'
import TapeInspector from './components/TapeInspector.vue'
import ParseErrorMap from './components/ParseErrorMap.vue'
import BenchmarkCi from './components/BenchmarkCi.vue'
import type { Theme } from 'vitepress'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('TapeFlowDiagram', TapeFlowDiagram)
    app.component('LazyLifecycle', LazyLifecycle)
    app.component('SimdPipeline', SimdPipeline)
    app.component('TreeVsTape', TreeVsTape)
    app.component('TapeInspector', TapeInspector)
    app.component('ParseErrorMap', ParseErrorMap)
    app.component('BenchmarkCi', BenchmarkCi)
  }
} satisfies Theme
