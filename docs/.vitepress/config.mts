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
            pageData.frontmatter.head.push(['link', { rel: 'canonical', href: canonicalUrl }])

            // BreadcrumbList — tells Google the hierarchy of every page so it can
            // show structured breadcrumbs instead of raw URLs in search snippets.
            // Segments: "guide/parsing.md" → Home › Guide › Parsing & Access
            const segments = pageData.relativePath
                .replace(/\.md$/, '')
                .replace(/\/index$/, '')
                .split('/')
                .filter(Boolean)

            const SECTION_LABELS: Record<string, string> = {
                guide: 'Guide',
                theory: 'Architecture',
                api: 'API Reference',
            }

            const items: { '@type': string; position: number; name: string; item: string }[] = [
                { '@type': 'ListItem', position: 1, name: 'Home', item: 'https://qbuem.com/qbuem-json/' }
            ]

            let accumulated = 'https://qbuem.com/qbuem-json'
            segments.forEach((seg, idx) => {
                accumulated += `/${seg}`
                const label = idx === 0
                    ? (SECTION_LABELS[seg] ?? (seg.charAt(0).toUpperCase() + seg.slice(1)))
                    : (pageData.title || seg.replace(/-/g, ' ').replace(/\b\w/g, c => c.toUpperCase()))
                items.push({
                    '@type': 'ListItem',
                    position: idx + 2,
                    name: label,
                    item: accumulated + (idx < segments.length - 1 ? '/' : '.html')
                })
            })

            if (items.length > 1) {
                pageData.frontmatter.head.push([
                    'script',
                    { type: 'application/ld+json' },
                    JSON.stringify({ '@context': 'https://schema.org', '@type': 'BreadcrumbList', itemListElement: items })
                ])
            }
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

            // AI-readable documentation index (llms.txt standard — https://llmstxt.org/)
            // Tells AI crawlers (GPTBot, ClaudeBot, PerplexityBot, etc.) where to find
            // structured documentation for RAG pipelines and LLM training.
            ['link', { rel: 'alternate', type: 'text/plain', title: 'LLMs.txt — AI-readable docs index', href: '/qbuem-json/llms.txt' }],

            // Preconnect — open TCP/TLS to font origins before CSS @import fires.
            // Cuts 100-300ms from first-contentful-paint on cold loads (Core Web Vitals).
            ['link', { rel: 'preconnect', href: 'https://fonts.googleapis.com' }],
            ['link', { rel: 'preconnect', href: 'https://fonts.gstatic.com', crossorigin: '' }],

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

            // JSON-LD — five graph nodes in one block for minimal HTTP overhead.
            //
            // 1. Organization — logo used in Knowledge Panels and rich results.
            // 2. WebSite + SearchAction — activates Sitelinks Searchbox.
            // 3. SoftwareApplication — richer than SoftwareSourceCode; enables
            //    applicationCategory, featureList, and screenshot in search snippets.
            // 4. SoftwareSourceCode — code-specific signals (repository, language).
            // 5. HowTo — enables rich "how to use" step snippets in Google Search.
            // 6. FAQPage — enables FAQ accordion rich results.
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
                        // SoftwareApplication enables richer snippets: applicationCategory,
                        // featureList, screenshot — these help both Google and AI models
                        // understand this is a developer library, not a website.
                        '@type': 'SoftwareApplication',
                        '@id': 'https://qbuem.com/qbuem-json/#application',
                        name: 'qbuem-json',
                        alternateName: ['qbuem json', 'qbuem-json library'],
                        description: 'qbuem-json is a header-only C++20 JSON library with dual-engine architecture. DOM engine: AVX-512/NEON SIMD, up to 2.9 GB/s parsing, 7.2 GB/s serialization. Nexus Fusion engine: zero-tape direct struct mapping, 50–230 ns latency. Single header file, zero dependencies, Apache 2.0.',
                        url: 'https://qbuem.com/qbuem-json/',
                        applicationCategory: 'DeveloperApplication',
                        applicationSubCategory: 'C++ Library',
                        operatingSystem: 'Linux, macOS',
                        version: '1.0.7',
                        softwareVersion: '1.0.7',
                        releaseNotes: 'https://github.com/qbuem/qbuem-json/releases/tag/v1.0.7',
                        downloadUrl: 'https://github.com/qbuem/qbuem-json/releases/tag/v1.0.7',
                        installUrl: 'https://qbuem.com/qbuem-json/guide/getting-started',
                        license: 'https://www.apache.org/licenses/LICENSE-2.0',
                        keywords: 'C++ JSON library, C++20 JSON, fastest JSON parser, SIMD JSON, AVX-512 JSON, zero-allocation JSON, high-performance JSON, HFT JSON, JSON serializer, single header JSON, header-only JSON, nlohmann alternative, simdjson alternative, RapidJSON alternative',
                        featureList: [
                            'AVX-512 and ARM NEON SIMD acceleration',
                            'Zero-allocation flat tape DOM',
                            'Nexus Fusion: direct JSON-to-struct mapping (zero tape)',
                            'Single header file, zero external dependencies',
                            'C++20 concepts-based API',
                            'RFC 8259, RFC 6901, RFC 6902 compliant',
                            'IEEE 754 round-trip float correctness',
                            '521 passing tests, 11 libFuzzer targets',
                            'STL container support (vector, map, optional, tuple, variant)',
                            'Up to 2.9 GB/s parsing, 7.2 GB/s serialization',
                            'Apache 2.0 license — free for commercial use',
                            'C, Python, Rust, Go language bindings'
                        ],
                        screenshot: {
                            '@type': 'ImageObject',
                            url: 'https://qbuem.com/qbuem-json/banner.png',
                            width: 989,
                            height: 232,
                            caption: 'qbuem-json — blazing-fast C++20 JSON library banner'
                        },
                        author: { '@id': 'https://qbuem.com/#organization' },
                        publisher: { '@id': 'https://qbuem.com/#organization' },
                        offers: {
                            '@type': 'Offer',
                            price: '0',
                            priceCurrency: 'USD'
                        }
                    },
                    {
                        '@type': 'SoftwareSourceCode',
                        '@id': 'https://qbuem.com/qbuem-json/#software',
                        name: 'qbuem-json',
                        description: 'The fastest C++20 JSON parser and serializer. Single header, zero dependencies, AVX-512 SIMD accelerated, zero-allocation design.',
                        url: 'https://qbuem.com/qbuem-json/',
                        codeRepository: 'https://github.com/qbuem/qbuem-json',
                        programmingLanguage: {
                            '@type': 'ComputerLanguage',
                            name: 'C++',
                            version: 'C++20',
                            url: 'https://en.cppreference.com/w/cpp/20'
                        },
                        runtimePlatform: 'C++20',
                        targetProduct: { '@id': 'https://qbuem.com/qbuem-json/#application' },
                        version: '1.0.7',
                        license: 'https://www.apache.org/licenses/LICENSE-2.0',
                        keywords: 'C++, JSON, SIMD, AVX-512, High-Performance, HFT, parser, serializer, zero-allocation',
                        author: { '@id': 'https://qbuem.com/#organization' }
                    },
                    {
                        // HowTo schema — enables step-by-step rich snippets for
                        // "how to use qbuem-json" queries in Google Search.
                        '@type': 'HowTo',
                        '@id': 'https://qbuem.com/qbuem-json/#howto',
                        name: 'How to use qbuem-json in a C++ project',
                        description: 'qbuem-json is a single-header C++20 JSON library. Add the header to your project, register your structs with QBUEM_JSON_FIELDS, then use qbuem::parse / qbuem::read / qbuem::fuse to deserialize and qbuem::write to serialize.',
                        image: 'https://qbuem.com/qbuem-json/banner.png',
                        totalTime: 'PT5M',
                        supply: [
                            { '@type': 'HowToSupply', name: 'C++20 compiler (GCC 13+, Clang 18+, or Apple Clang)' },
                            { '@type': 'HowToSupply', name: 'qbuem_json.hpp (single header file)' }
                        ],
                        step: [
                            {
                                '@type': 'HowToStep',
                                position: 1,
                                name: 'Download the header',
                                text: 'Copy qbuem_json.hpp into your project: wget https://raw.githubusercontent.com/qbuem/qbuem-json/main/include/qbuem_json/qbuem_json.hpp',
                                url: 'https://qbuem.com/qbuem-json/guide/getting-started#option-a-single-header-drop-in'
                            },
                            {
                                '@type': 'HowToStep',
                                position: 2,
                                name: 'Include the header and parse JSON',
                                text: 'Include <qbuem_json/qbuem_json.hpp>. Create a qbuem::Document and call qbuem::parse(doc, json_string) to get a qbuem::Value root. Access fields with root["key"].as<T>().',
                                url: 'https://qbuem.com/qbuem-json/guide/parsing'
                            },
                            {
                                '@type': 'HowToStep',
                                position: 3,
                                name: 'Register your struct with QBUEM_JSON_FIELDS',
                                text: 'Define your struct, then add QBUEM_JSON_FIELDS(MyStruct, field1, field2, ...) outside the struct at namespace scope. Use qbuem::read<MyStruct>(json) or qbuem::fuse<MyStruct>(json) to deserialize.',
                                url: 'https://qbuem.com/qbuem-json/guide/mapping'
                            },
                            {
                                '@type': 'HowToStep',
                                position: 4,
                                name: 'Serialize to JSON',
                                text: 'Call qbuem::write(my_struct) to get a compact JSON string, or qbuem::write(my_struct, 2) for pretty-printed output. For parsed documents use value.dump().',
                                url: 'https://qbuem.com/qbuem-json/guide/serialization'
                            },
                            {
                                '@type': 'HowToStep',
                                position: 5,
                                name: 'Compile with C++20',
                                text: 'Compile with: g++ -std=c++20 -O3 -march=native my_app.cpp -o my_app',
                                url: 'https://qbuem.com/qbuem-json/guide/getting-started#build-configuration'
                            }
                        ]
                    },
                    {
                        // FAQPage schema — enables FAQ accordion rich results in Google Search.
                        // These questions are the most common ones developers ask when
                        // evaluating or starting with a new JSON library.
                        '@type': 'FAQPage',
                        '@id': 'https://qbuem.com/qbuem-json/#faq',
                        mainEntity: [
                            {
                                '@type': 'Question',
                                name: 'What is qbuem-json?',
                                acceptedAnswer: {
                                    '@type': 'Answer',
                                    text: 'qbuem-json is a header-only C++20 JSON library with two engines: the DOM engine (flat tape, AVX-512/NEON SIMD, up to 2.9 GB/s parsing) and Nexus Fusion (zero-tape direct struct mapping, 50–230 ns). It is distributed as a single header file with zero external dependencies and is licensed under Apache 2.0.'
                                }
                            },
                            {
                                '@type': 'Question',
                                name: 'How do I install qbuem-json?',
                                acceptedAnswer: {
                                    '@type': 'Answer',
                                    text: 'Option A (recommended): copy qbuem_json.hpp into your project with: wget https://raw.githubusercontent.com/qbuem/qbuem-json/main/include/qbuem_json/qbuem_json.hpp. Option B: use CMake FetchContent. Option C: clone the repository and build with cmake. All options require a C++20 compiler (GCC 13+, Clang 18+, or Apple Clang).'
                                }
                            },
                            {
                                '@type': 'Question',
                                name: 'How do I parse JSON with qbuem-json?',
                                acceptedAnswer: {
                                    '@type': 'Answer',
                                    text: 'Create a qbuem::Document and call qbuem::parse(doc, json_string). Access values with root["key"].as<T>(). For struct mapping, add QBUEM_JSON_FIELDS(MyStruct, fields...) outside the struct and call qbuem::read<MyStruct>(json) or qbuem::fuse<MyStruct>(json) for zero-tape mapping.'
                                }
                            },
                            {
                                '@type': 'Question',
                                name: 'How does qbuem-json compare to nlohmann/json, simdjson, and RapidJSON?',
                                acceptedAnswer: {
                                    '@type': 'Answer',
                                    text: 'qbuem-json outperforms all three on parsing and serialization benchmarks. It achieves 2.9 GB/s parsing vs simdjson ~2.5 GB/s, yyjson ~2.3 GB/s, RapidJSON ~0.8 GB/s, and nlohmann ~0.3 GB/s. It also offers direct struct mapping via Nexus Fusion (50–230 ns) which has no equivalent in nlohmann or RapidJSON.'
                                }
                            },
                            {
                                '@type': 'Question',
                                name: 'What is the QBUEM_JSON_FIELDS macro and where should I put it?',
                                acceptedAnswer: {
                                    '@type': 'Answer',
                                    text: 'QBUEM_JSON_FIELDS(StructName, field1, field2, ...) registers a struct for automatic JSON serialization and deserialization. It must be placed OUTSIDE the struct body at namespace scope. Placing it inside the struct breaks ADL (Argument-Dependent Lookup) and will cause compile errors. It supports up to 32 fields and works with nested structs and STL containers automatically.'
                                }
                            },
                            {
                                '@type': 'Question',
                                name: 'What is Nexus Fusion and when should I use it?',
                                acceptedAnswer: {
                                    '@type': 'Answer',
                                    text: 'Nexus Fusion is qbuem-json\'s zero-tape engine. Instead of building a DOM tape first, it streams JSON bytes directly into struct fields using compile-time FNV-1a key hashing for O(1) dispatch. Use qbuem::fuse<T>() instead of qbuem::read<T>() when you need minimum latency (50–230 ns), such as in HFT tick data processing or real-time game state. Requires QBUEM_JSON_FIELDS registration.'
                                }
                            },
                            {
                                '@type': 'Question',
                                name: 'Does qbuem-json support Windows?',
                                acceptedAnswer: {
                                    '@type': 'Answer',
                                    text: 'No. qbuem-json supports Linux (x86_64 and aarch64) and macOS (Apple Silicon) only. Windows is not supported.'
                                }
                            },
                            {
                                '@type': 'Question',
                                name: 'Is qbuem-json free for commercial use?',
                                acceptedAnswer: {
                                    '@type': 'Answer',
                                    text: 'Yes. qbuem-json is released under the Apache License 2.0, which is a permissive open-source license that allows commercial use, modification, and distribution.'
                                }
                            }
                        ]
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
                    text: 'v1.0.7',
                    items: [
                        { text: 'Release Notes', link: 'https://github.com/qbuem/qbuem-json/releases/tag/v1.0.7' },
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
