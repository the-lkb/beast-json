import { defineConfig } from 'vitepress'
import { withMermaid } from 'vitepress-plugin-mermaid'

export default withMermaid(
    defineConfig({
        title: "Beast JSON",
        description: "The Ultimate High-Performance C++20 JSON Engine",
        base: '/beast-json/',
        sitemap: {
            hostname: 'https://the-lkb.github.io/beast-json/'
        },
        head: [
            ['meta', { name: 'keywords', content: 'C++, JSON, SIMD, AVX-512, High-Performance, HFT, parser, serializer' }],
            ['meta', { property: 'og:title', content: 'Beast JSON - Ultimate C++20 JSON Engine' }],
            ['meta', { property: 'og:description', content: 'Bleeding-edge C++20 JSON library with AVX-512 acceleration and zero-allocation design.' }],
            ['meta', { property: 'og:image', content: 'https://the-lkb.github.io/beast-json/logo.png' }],
            ['meta', { property: 'og:type', content: 'website' }],
            ['meta', { name: 'twitter:card', content: 'summary_large_image' }]
        ],
        themeConfig: {
            logo: '/logo.png',
            nav: [
                { text: 'Guide', link: '/guide/introduction' },
                { text: 'Architecture', link: '/theory/architecture' },
                { text: 'Benchmarks', link: '/guide/benchmarks' },
                { text: 'API Reference', link: '/api/index' }
            ],

            sidebar: {
                '/guide/': [
                    {
                        text: 'Introduction',
                        items: [
                            { text: 'What is Beast JSON?', link: '/guide/introduction' },
                            { text: 'Getting Started', link: '/guide/getting-started' },
                            { text: 'Vision & Roadmap', link: '/guide/vision' }
                        ]
                    },
                    {
                        text: 'Usage Guide',
                        items: [
                            { text: 'Parsing & Access', link: '/guide/parsing' },
                            { text: 'Serialization', link: '/guide/serialization' },
                            { text: 'Object Mapping (Macros)', link: '/guide/mapping' },
                            { text: 'Error Handling', link: '/guide/errors' }
                        ]
                    },
                    {
                        text: 'Advanced',
                        items: [
                            { text: 'Custom Allocators', link: '/guide/allocators' },
                            { text: 'Language Bindings', link: '/guide/bindings' },
                            { text: 'HFT Optimization Patterns', link: '/guide/hft-patterns' }
                        ]
                    }
                ],
                '/theory/': [
                    {
                        text: 'Engineering Theory',
                        items: [
                            { text: 'The Tape Architecture', link: '/theory/architecture' },
                            { text: 'SIMD Acceleration', link: '/theory/simd' },
                            { text: 'The Russ Cox Algorithm', link: '/theory/russ-cox' },
                            { text: 'Zero-Allocation Principle', link: '/theory/zero-allocation' }
                        ]
                    }
                ]
            },

            socialLinks: [
                { icon: 'github', link: 'https://github.com/the-lkb/beast-json' }
            ],

            footer: {
                message: 'Released under the Apache 2.0 License.',
                copyright: 'Copyright © 2024-2026 Beast JSON Authors'
            }
        },
        mermaid: {
            // mermaid config
        }
    })
)
