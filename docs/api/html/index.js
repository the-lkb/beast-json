var index =
[
    [ "1. Introduction", "index.html#autotoc_md5", null ],
    [ "2. Performance Benchmarks", "index.html#autotoc_md7", [
      [ "2.1 Linux x86_64 (GCC 13.3, AVX-512, PGO)", "index.html#autotoc_md8", null ],
      [ "2.2 Linux AArch64 (GCC/Clang)", "index.html#autotoc_md9", null ],
      [ "2.3 Apple Silicon (macOS, Apple Clang, NEON, PGO)", "index.html#autotoc_md10", null ],
      [ "2.4 Sub-MegaByte Memory Efficiency", "index.html#autotoc_md11", null ],
      [ "2.5 Extreme Heavy-Load Benchmarks (Harsh Environment)", "index.html#autotoc_md12", null ]
    ] ],
    [ "3. Architecture &amp; Internals", "index.html#autotoc_md14", [
      [ "3.1 TapeNode Layout", "index.html#autotoc_md15", null ],
      [ "3.2 Two-Phase Parser (x86_64 &lt;= 2MB)", "index.html#autotoc_md16", null ],
      [ "3.3 SWAR String Scanning", "index.html#autotoc_md17", null ],
      [ "3.4 KeyLenCache", "index.html#autotoc_md18", null ]
    ] ],
    [ "4. API Reference", "index.html#autotoc_md20", [
      [ "4.1 <span class=\"tt\">beast::Document</span> and <span class=\"tt\">beast::Value</span>", "index.html#autotoc_md21", null ],
      [ "4.2 Non-Destructive Mutations", "index.html#autotoc_md22", null ],
      [ "4.3 Iteration and C++20 Ranges", "index.html#autotoc_md23", null ]
    ] ],
    [ "5. Auto-Serialization Macro", "index.html#autotoc_md25", [
      [ "5.1 Custom Third-Party Types via ADL", "index.html#autotoc_md26", null ]
    ] ],
    [ "6. RFC 8259 Validator", "index.html#autotoc_md28", null ],
    [ "7. Language Bindings", "index.html#autotoc_md30", [
      [ "Python Example", "index.html#autotoc_md31", null ]
    ] ],
    [ "8. Optimization Failures &amp; Lessons", "index.html#autotoc_md33", [
      [ "8.1 AArch64", "index.html#autotoc_md34", null ],
      [ "8.2 x86_64", "index.html#autotoc_md35", null ],
      [ "8.3 Apple Silicon PGO/LTO Golden Rules", "index.html#autotoc_md36", null ]
    ] ],
    [ "9. Development Roadmap &amp; History", "index.html#autotoc_md38", null ],
    [ "10. Security &amp; Memory-Safety Hardening", "index.html#autotoc_md39", [
      [ "10.1 Vulnerability Report", "index.html#autotoc_md40", [
        [ "Bug 1 — Null-Dereference in <span class=\"tt\">skip_to_action()</span> <em>(heap-buffer-overflow)</em>", "index.html#autotoc_md41", null ],
        [ "Bug 2 — Empty-Tape Read via Bare Separator <em>(heap-buffer-overflow)</em>", "index.html#autotoc_md42", null ],
        [ "Bug 3 — Non-String Object Keys + Iterator Out-of-Bounds <em>(heap-buffer-overflow)</em>", "index.html#autotoc_md43", null ],
        [ "Bug 4 — Stale Overlay Maps + Stack Underflow in <span class=\"tt\">dump_changes_()</span> <em>(UBSan / stack-buffer-underflow)</em>", "index.html#autotoc_md44", null ],
        [ "Bug 5 — <span class=\"tt\">skip_value_()</span> Out-of-Bounds + <span class=\"tt\">memcpy</span> Past Source End <em>(heap-buffer-overflow)</em>", "index.html#autotoc_md45", null ]
      ] ],
      [ "10.2 Fuzz Infrastructure", "index.html#autotoc_md46", null ]
    ] ]
];