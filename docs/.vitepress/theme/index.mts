import DefaultTheme from 'vitepress/theme'
import './custom.css'
import TapeFlowDiagram from './components/TapeFlowDiagram.vue'
import LazyLifecycle from './components/LazyLifecycle.vue'
import SimdPipeline from './components/SimdPipeline.vue'
import TreeVsTape from './components/TreeVsTape.vue'
import type { Theme } from 'vitepress'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('TapeFlowDiagram', TapeFlowDiagram)
    app.component('LazyLifecycle', LazyLifecycle)
    app.component('SimdPipeline', SimdPipeline)
    app.component('TreeVsTape', TreeVsTape)
  }
} satisfies Theme
