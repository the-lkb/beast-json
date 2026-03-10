import { defineConfig } from 'vitepress'
import { withMermaid } from 'vitepress-plugin-mermaid'

export default withMermaid(
    defineConfig({
        title: "Beast JSON",
        description: "Beast JSON is the fastest C++20 JSON parser and serializer. Single header, zero dependencies, AVX-512 SIMD accelerated, zero-allocation design for HFT, game engines, and high-throughput servers.",
        base: '/beast-json/',
        lang: 'en-US',
        appearance: false,
        sitemap: {
            hostname: 'https://qbuem.github.io/beast-json/'
        },
        markdown: {
            math: true
        },
        head: [
            // Google Site Verification
            ['meta', { name: 'google-site-verification', content: 'lyhYqUe6A757oe9CdwPEGxsyL7jHnqJ87ssXVuJdE_k' }],

            // SEO - Core
            ['meta', { name: 'keywords', content: 'C++ JSON, C++20 JSON library, fastest JSON parser, SIMD JSON, AVX-512 JSON, zero-allocation JSON, high-performance JSON, HFT JSON, JSON serializer, single header JSON, beast-json, nlohmann alternative, simdjson alternative, RapidJSON alternative' }],
            ['meta', { name: 'author', content: 'Beast JSON Authors' }],
            ['meta', { name: 'robots', content: 'index, follow' }],
            ['link', { rel: 'canonical', href: 'https://qbuem.github.io/beast-json/' }],

            // Open Graph
            ['meta', { property: 'og:title', content: 'Beast JSON - The Fastest C++20 JSON Engine' }],
            ['meta', { property: 'og:description', content: 'Beast JSON is a bleeding-edge C++20 JSON library with AVX-512 SIMD acceleration, zero-allocation design, and single-header simplicity. Up to 2.7 GB/s parsing, 8.1 GB/s serialization.' }],
            ['meta', { property: 'og:image', content: 'https://qbuem.github.io/beast-json/logo.png' }],
            ['meta', { property: 'og:image:alt', content: 'Beast JSON Logo' }],
            ['meta', { property: 'og:type', content: 'website' }],
            ['meta', { property: 'og:url', content: 'https://qbuem.github.io/beast-json/' }],
            ['meta', { property: 'og:site_name', content: 'Beast JSON' }],
            ['meta', { property: 'og:locale', content: 'en_US' }],

            // Twitter Card
            ['meta', { name: 'twitter:card', content: 'summary_large_image' }],
            ['meta', { name: 'twitter:title', content: 'Beast JSON - The Fastest C++20 JSON Engine' }],
            ['meta', { name: 'twitter:description', content: 'Bleeding-edge C++20 JSON library: AVX-512 SIMD, zero-allocation, single header. Outperforms simdjson, yyjson, RapidJSON, and nlohmann.' }],
            ['meta', { name: 'twitter:image', content: 'https://qbuem.github.io/beast-json/logo.png' }],

            // JSON-LD Structured Data
            ['script', { type: 'application/ld+json' }, JSON.stringify({
                '@context': 'https://schema.org',
                '@type': 'SoftwareSourceCode',
                name: 'Beast JSON',
                description: 'The fastest C++20 JSON parser and serializer. Single header, zero dependencies, AVX-512 SIMD accelerated, zero-allocation design.',
                url: 'https://qbuem.github.io/beast-json/',
                codeRepository: 'https://github.com/qbuem/beast-json',
                programmingLanguage: 'C++',
                runtimePlatform: 'C++20',
                version: '1.0.5',
                license: 'https://www.apache.org/licenses/LICENSE-2.0',
                keywords: 'C++, JSON, SIMD, AVX-512, High-Performance, HFT, parser, serializer, zero-allocation',
                offers: {
                    '@type': 'Offer',
                    price: '0',
                    priceCurrency: 'USD'
                }
            })]
        ],
        themeConfig: {
            logo: '/logo.png',
            nav: [
                { text: 'Guide', link: '/guide/introduction' },
                { text: 'Architecture', link: '/theory/architecture' },
                { text: 'Benchmarks', link: '/guide/benchmarks' },
                { text: 'API Reference', link: '/api/index' },
                {
                    text: 'v1.0.5',
                    items: [
                        { text: 'Release Notes', link: 'https://github.com/qbuem/beast-json/releases/tag/v1.0.5' },
                        { text: 'All Releases', link: 'https://github.com/qbuem/beast-json/releases' }
                    ]
                }
            ],

            sidebar: {
                '/guide/': [
                    {
                        text: 'Introduction',
                        items: [
                            { text: 'What is Beast JSON?', link: '/guide/introduction' },
                            { text: 'Getting Started', link: '/guide/getting-started' },
                            { text: 'Vision & Roadmap', link: '/guide/vision' },
                            { text: 'Acknowledgments', link: '/guide/acknowledgments' }
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
                            { text: 'Low-Latency Patterns', link: '/guide/low-latency-patterns' },
                            { text: 'Debugging Guide', link: '/guide/debugging' }
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
                { icon: 'github', link: 'https://github.com/qbuem/beast-json' }
            ],

            footer: {
                message: 'Released under the Apache 2.0 License.',
                copyright: 'Copyright © 2026 Beast JSON Authors'
            }
        },
        mermaid: {
            // Allow HTML in node labels so <br/> renders as a line-break
            // rather than being stripped/shown as raw text, which causes
            // nodes to overflow their bounding boxes.
            securityLevel: 'loose',
        }
    })
)
