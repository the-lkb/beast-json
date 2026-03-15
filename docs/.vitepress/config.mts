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
            hostname: 'https://qbuem.com/qbuem-json/'
        },
        markdown: {
            math: true
        },
        transformPageData(pageData) {
            const canonicalUrl = `https://qbuem.com/qbuem-json/${pageData.relativePath}`
                .replace(/index\.md$/, '')
                .replace(/\.md$/, '.html')

            pageData.frontmatter.head ??= []
            pageData.frontmatter.head.push([
                'link',
                { rel: 'canonical', href: canonicalUrl }
            ])
        },
        head: [
            // Domain Redirect (github.io -> qbuem.com)
            ['script', {}, `
                (function() {
                    if (window.location.hostname === 'qbuem.github.io') {
                        var newUrl = window.location.href.replace('qbuem.github.io', 'qbuem.com');
                        window.location.replace(newUrl);
                    }
                })();
            `],

            // Google Site Verification
            ['meta', { name: 'google-site-verification', content: 'lyhYqUe6A757oe9CdwPEGxsyL7jHnqJ87ssXVuJdE_k' }],

            // Favicon — Google uses these to display the site icon in search result snippets.
            // SVG is preferred by modern browsers; ICO/PNG fallbacks cover older clients.
            ['link', { rel: 'icon', type: 'image/svg+xml', href: '/qbuem-json/logo.svg' }],
            ['link', { rel: 'icon', type: 'image/png', sizes: '263x234', href: '/qbuem-json/logo.png' }],
            ['link', { rel: 'apple-touch-icon', sizes: '263x234', href: '/qbuem-json/logo.png' }],
            ['meta', { name: 'msapplication-TileImage', content: '/qbuem-json/logo.png' }],
            ['meta', { name: 'theme-color', content: '#1a2744' }],

            // SEO - Core
            ['meta', { name: 'keywords', content: 'C++ JSON, C++20 JSON library, fastest JSON parser, SIMD JSON, AVX-512 JSON, zero-allocation JSON, high-performance JSON, HFT JSON, JSON serializer, single header JSON, qbuem-json, nlohmann alternative, simdjson alternative, RapidJSON alternative' }],
            ['meta', { name: 'author', content: 'qbuem-json Authors' }],
            ['meta', { name: 'robots', content: 'index, follow, max-image-preview:large' }],

            // Open Graph
            ['meta', { property: 'og:title', content: 'qbuem-json — Feel the Power of Ultimate JSON Speed' }],
            ['meta', { property: 'og:description', content: 'qbuem-json: small changes, big future. Bleeding-edge C++20 JSON library with AVX-512 SIMD acceleration, zero-allocation design, and single-header simplicity. Up to 2.7 GB/s parsing, 8.1 GB/s serialization.' }],
            ['meta', { property: 'og:image', content: 'https://qbuem.com/qbuem-json/banner.png' }],
            ['meta', { property: 'og:image:width', content: '989' }],
            ['meta', { property: 'og:image:height', content: '232' }],
            ['meta', { property: 'og:image:type', content: 'image/png' }],
            ['meta', { property: 'og:image:alt', content: 'qbuem-json — blazing-fast C++20 JSON library' }],
            ['meta', { property: 'og:type', content: 'website' }],
            ['meta', { property: 'og:url', content: 'https://qbuem.com/qbuem-json/' }],
            ['meta', { property: 'og:site_name', content: 'qbuem-json' }],
            ['meta', { property: 'og:locale', content: 'en_US' }],

            // Twitter Card
            ['meta', { name: 'twitter:card', content: 'summary_large_image' }],
            ['meta', { name: 'twitter:title', content: 'qbuem-json — Feel the Power of Ultimate JSON Speed' }],
            ['meta', { name: 'twitter:description', content: 'qbuem-json: small changes, big future. AVX-512 SIMD, zero-allocation, single header C++20. Outperforms simdjson, yyjson, RapidJSON, and nlohmann.' }],
            ['meta', { name: 'twitter:image', content: 'https://qbuem.com/qbuem-json/banner.png' }],
            ['meta', { name: 'twitter:image:alt', content: 'qbuem-json — blazing-fast C++20 JSON library' }],

            // JSON-LD — three graph nodes in one block for minimal HTTP overhead.
            //
            // 1. Organization — the "logo" property here is what Google displays in
            //    Knowledge Panels and rich results. Image must be ≥112×112px and
            //    accessible without auth (logo.png: 263×234 ✓).
            //
            // 2. WebSite + SearchAction — activates the Sitelinks Searchbox in Google
            //    when the site gains enough authority. The query_input target points at
            //    VitePress's built-in search endpoint.
            //
            // 3. SoftwareSourceCode — preserves the existing software-specific signals.
            ['script', { type: 'application/ld+json' }, JSON.stringify({
                '@context': 'https://schema.org',
                '@graph': [
                    {
                        '@type': 'Organization',
                        '@id': 'https://qbuem.com/#organization',
                        name: 'qbuem',
                        url: 'https://qbuem.com/',
                        logo: {
                            '@type': 'ImageObject',
                            '@id': 'https://qbuem.com/qbuem-json/logo.png',
                            url: 'https://qbuem.com/qbuem-json/logo.png',
                            width: 263,
                            height: 234,
                            caption: 'qbuem-json logo'
                        },
                        sameAs: [
                            'https://github.com/qbuem'
                        ]
                    },
                    {
                        '@type': 'WebSite',
                        '@id': 'https://qbuem.com/qbuem-json/#website',
                        url: 'https://qbuem.com/qbuem-json/',
                        name: 'qbuem-json',
                        description: 'The fastest C++20 JSON parser and serializer. AVX-512 SIMD accelerated, zero-allocation, single header.',
                        publisher: { '@id': 'https://qbuem.com/#organization' },
                        potentialAction: {
                            '@type': 'SearchAction',
                            target: {
                                '@type': 'EntryPoint',
                                urlTemplate: 'https://qbuem.com/qbuem-json/?search={search_term_string}'
                            },
                            'query-input': 'required name=search_term_string'
                        }
                    },
                    {
                        '@type': 'SoftwareSourceCode',
                        '@id': 'https://qbuem.com/qbuem-json/#software',
                        name: 'qbuem-json',
                        description: 'The fastest C++20 JSON parser and serializer. Single header, zero dependencies, AVX-512 SIMD accelerated, zero-allocation design.',
                        url: 'https://qbuem.com/qbuem-json/',
                        codeRepository: 'https://github.com/qbuem/qbuem-json',
                        programmingLanguage: 'C++',
                        runtimePlatform: 'C++20',
                        version: '1.0.6',
                        license: 'https://www.apache.org/licenses/LICENSE-2.0',
                        keywords: 'C++, JSON, SIMD, AVX-512, High-Performance, HFT, parser, serializer, zero-allocation',
                        author: { '@id': 'https://qbuem.com/#organization' },
                        offers: {
                            '@type': 'Offer',
                            price: '0',
                            priceCurrency: 'USD'
                        }
                    }
                ]
            })]
        ],
        themeConfig: {
            logo: '/logo.png',
            nav: [
                { text: 'Guide', link: '/guide/introduction' },
                { text: 'Architecture', link: '/theory/architecture' },
                { text: 'Benchmarks', link: '/guide/benchmarks' },
                { text: 'Correctness', link: '/guide/correctness' },
                {
                    text: 'API Reference',
                    items: [
                        { text: 'Manual Reference', link: '/api/index' },
                        { text: 'Doxygen (Auto-generated)', link: '/api/reference/index.html', target: '_blank', rel: 'noreferrer' }
                    ]
                },
                {
                    text: 'v1.0.6',
                    items: [
                        { text: 'Release Notes', link: 'https://github.com/qbuem/qbuem-json/releases/tag/v1.0.6' },
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
                            { text: 'Correctness & Safety', link: '/guide/correctness' },
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
