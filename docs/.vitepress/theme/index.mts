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
    // Using a capture-phase click listener (fires before Vue's handlers) so
    // the SPA router never sees the click. link.href gives the fully-resolved
    // absolute URL, correctly including the site's base path (/qbuem-json/).
    if (typeof window !== 'undefined') {
      document.addEventListener('click', (e) => {
        const link = (e.target as Element).closest('a') as HTMLAnchorElement | null
        if (link?.href.includes('/api/reference/')) {
          e.preventDefault()
          e.stopImmediatePropagation()
          window.location.href = link.href
        }
      }, true) // true = capture phase
    }
  }
} satisfies Theme
