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
  enhanceApp({ app, router }) {
    app.component('TapeFlowDiagram', TapeFlowDiagram)
    app.component('LazyLifecycle', LazyLifecycle)
    app.component('SimdPipeline', SimdPipeline)
    app.component('TreeVsTape', TreeVsTape)
    app.component('TapeInspector', TapeInspector)
    app.component('ParseErrorMap', ParseErrorMap)
    app.component('BenchmarkCi', BenchmarkCi)

    // Doxygen pages are static HTML served outside VitePress's route system.
    // Without this guard, clicking a /api/reference/ link from within
    // VitePress content causes the SPA router to handle it, fail to find a
    // matching route, and render a 404. Returning false cancels SPA navigation
    // and lets the browser make a full HTTP request to the real file.
    if (typeof window !== 'undefined') {
      router.onBeforeRouteChange = (to: string) => {
        if (to.includes('/api/reference/')) {
          window.location.href = to
          return false
        }
      }
    }
  }
} satisfies Theme
