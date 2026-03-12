import { defineConfig } from 'vitepress'
import { withMermaid } from 'vitepress-plugin-mermaid'

export default withMermaid(
    defineConfig({
        title: "qbuem-json",
        description: "qbuem-json — Feel the Power of Ultimate JSON Speed. The fastest C++20 JSON parser and serializer. Single header, zero dependencies, AVX-512 SIMD accelerated, zero-allocation design. small changes, big future.",
        base: '/qbuem-json/',
        lang: 'en-US',
        appearance: false,
        ignoreDeadLinks: [/\/api\/reference/],
        sitemap: {
            hostname: 'https://qbuem.github.io/qbuem-json/'
        },
        markdown: {
            math: true
        },
        head: [
            // Google Site Verification
            ['meta', { name: 'google-site-verification', content: 'lyhYqUe6A757oe9CdwPEGxsyL7jHnqJ87ssXVuJdE_k' }],

            // SEO - Core
            ['meta', { name: 'keywords', content: 'C++ JSON, C++20 JSON library, fastest JSON parser, SIMD JSON, AVX-512 JSON, zero-allocation JSON, high-performance JSON, HFT JSON, JSON serializer, single header JSON, qbuem-json, nlohmann alternative, simdjson alternative, RapidJSON alternative' }],
            ['meta', { name: 'author', content: 'qbuem-json Authors' }],
            ['meta', { name: 'robots', content: 'index, follow' }],
            ['link', { rel: 'canonical', href: 'https://qbuem.github.io/qbuem-json/' }],

            // Open Graph
            ['meta', { property: 'og:title', content: 'qbuem-json — Feel the Power of Ultimate JSON Speed' }],
            ['meta', { property: 'og:description', content: 'qbuem-json: small changes, big future. Bleeding-edge C++20 JSON library with AVX-512 SIMD acceleration, zero-allocation design, and single-header simplicity. Up to 2.7 GB/s parsing, 8.1 GB/s serialization.' }],
            ['meta', { property: 'og:image', content: 'https://qbuem.github.io/qbuem-json/banner.svg' }],
            ['meta', { property: 'og:image:alt', content: 'qbuem-json Logo' }],
            ['meta', { property: 'og:type', content: 'website' }],
            ['meta', { property: 'og:url', content: 'https://qbuem.github.io/qbuem-json/' }],
            ['meta', { property: 'og:site_name', content: 'qbuem-json' }],
            ['meta', { property: 'og:locale', content: 'en_US' }],

            // Twitter Card
            ['meta', { name: 'twitter:card', content: 'summary_large_image' }],
            ['meta', { name: 'twitter:title', content: 'qbuem-json — Feel the Power of Ultimate JSON Speed' }],
            ['meta', { name: 'twitter:description', content: 'qbuem-json: small changes, big future. AVX-512 SIMD, zero-allocation, single header C++20. Outperforms simdjson, yyjson, RapidJSON, and nlohmann.' }],
            ['meta', { name: 'twitter:image', content: 'https://qbuem.github.io/qbuem-json/banner.svg' }],

            // JSON-LD Structured Data
            ['script', { type: 'application/ld+json' }, JSON.stringify({
                '@context': 'https://schema.org',
                '@type': 'SoftwareSourceCode',
                name: 'qbuem-json',
                description: 'The fastest C++20 JSON parser and serializer. Single header, zero dependencies, AVX-512 SIMD accelerated, zero-allocation design.',
                url: 'https://qbuem.github.io/qbuem-json/',
                codeRepository: 'https://github.com/qbuem/qbuem-json',
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
            logo: '/logo.svg',
            nav: [
                { text: 'Guide', link: '/guide/introduction' },
                { text: 'Architecture', link: '/theory/architecture' },
                { text: 'Benchmarks', link: '/guide/benchmarks' },
                {
                    text: 'API Reference',
                    items: [
                        { text: 'Manual Reference', link: '/api/index' },
                        { text: 'Doxygen (Auto-generated)', link: '/api/reference/index.html', target: '_blank', rel: 'noreferrer' }
                    ]
                },
                {
                    text: 'v1.0.5',
                    items: [
                        { text: 'Release Notes', link: 'https://github.com/qbuem/qbuem-json/releases/tag/v1.0.5' },
                        { text: 'All Releases', link: 'https://github.com/qbuem/qbuem-json/releases' }
                    ]
                }
            ],

            sidebar: {
                '/guide/': [
                    {
                        text: 'Introduction',
                        items: [
                            { text: 'What is qbuem-json?', link: '/guide/introduction' },
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
                            { text: 'Numeric Serialization (Schubfach + yy-itoa)', link: '/theory/numeric-serialization' },
                            { text: 'The Russ Cox Algorithm', link: '/theory/russ-cox' },
                            { text: 'Zero-Allocation Principle', link: '/theory/zero-allocation' },
                            { text: 'Nexus Fusion (Zero-Tape)', link: '/theory/nexus-fusion' }
                        ]
                    }
                ]
            },

            socialLinks: [
                { icon: 'github', link: 'https://github.com/qbuem/qbuem-json' }
            ],

            footer: {
                message: 'Released under the Apache 2.0 License. · <em>small changes, big future</em>',
                copyright: 'Copyright © 2026 qbuem · qbuem-json Authors'
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
