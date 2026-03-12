# Vision & Roadmap

## Roadmap

### Phase 1: Foundations ✅
- ✅ Zero-Allocation Tape DOM.
- ✅ **Nexus Fusion (Zero-Tape)**: Direct JSON-to-struct mapping.
- ✅ AVX-512 & ARM NEON structural indexing.
- ✅ Russ Cox unrounded scaling (2026 update).
- ✅ C++20 Concepts-based API.

### Phase 2: Compliance & Reliability ✅
- ✅ Strict RFC 6901 (JSON Pointer) compliance.
- ✅ Strict RFC 6902 (JSON Patch) with transactional safety.
- ✅ Hardened security via ASan/UBSan/TSan.

### Phase 3: Advanced Ecosystem (2026–)
- ⬜ **SVE (Scalable Vector Extension)** support for next-gen ARM architectures.
- ⬜ **JSON Schema Validation**: High-performance validation engine built on the Tape DOM.
- ⬜ **Language Bindings**: Native-speed wrappers for Python (using nanobind) and Go.
- ⬜ **Custom Backend Integration**: Easier hooks for custom memory resources (std::pmr).

### Phase 4: Extreme Optimization (Ongoing)
- ⬜ Further micro-optimization of the Russ Cox algorithm via inline assembly where beneficial.
- ⬜ Reduced binary footprint for embedded/IoT constraints.
- ⬜ Predictive parsing hints for known schema structures.
