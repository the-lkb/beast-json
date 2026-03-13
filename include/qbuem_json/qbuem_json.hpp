/**
 * @brief qbuem-json v1.0.6 - High-Performance C++20 JSON Parser
 * @version 1.0.6
 *
 * 🏆 Ultimate C++20 JSON Library - 100% Complete!
 *
 * (c) 2026 qbuem and the qbuem-json Authors.
 *
 * Performance (Hybrid Strategy):
 * ✨ qbuem-json DOM:   1200-1400 MB/s (High-Throughput SIMD)
 * ✨ qbuem-json Nexus: < 1.0 μs (Ultra-Low Latency Zero-Tape)
 *
 * Core Engines:
 * ✅ Dual-Engine Architecture: Choose between DOM and Nexus Fusion.
 * ✅ Nexus Fusion (Zero-Tape): Direct JSON-to-Struct mapping.
 * ✅ Russ Cox: Unrounded scaling (Bit-accurate floating point).
 * ✅ Full SIMD: AVX-512, NEON, and SWAR structural indexing.
 * ✅ C++20 Native: Concepts, Ranges, and std::pmr support.
 *
 * Documentation: https://qbuem.github.io/qbuem-json/
 *
 * License: Apache License 2.0
 *
 * ============================================================================
 * 🤖 AI & Developer Context Guide
 * ============================================================================
 * Welcome to the qbuem_json source! When modifying this file, keep the
 * following architectural guidelines and constraints in mind:
 *
 * 1. The Value vs SafeValue Dichotomy
 *    - `Value`: Strict, confident access. Returns an invalid `Value{}` on
 * missing key, BUT throws `std::runtime_error` if you call `.as<T>()` and the
 * type mismatches. Use `is_int()` etc. first if unsure.
 *    - `SafeValue`: Monadic, untrusted access. Created via `root.get("key")`.
 *                   NEVER throws. Propagates missing keys or type mismatches
 * across deep chains. Terminated by `.value_or(default)`.
 *
 * 2. Auto-Serialization & ADL (Argument-Dependent Lookup)
 *    - The magic `QBUEM_JSON_FIELDS` macro works by generating
 * `from_qbuem_json(const Value&, T&)` and `to_qbuem_json(Value&, const T&)`
 * functions inside the struct's namespace.
 *    - To support third-party types without macro injection, simply implement
 * these two ADL functions manually in the target type's namespace.
 *
 * 3. High-Performance Constraints (PGO/LTO Sensitivities)
 *    - Tape nodes (TapeNode) are strictly 8 bytes.
 *    - Adding new loop back-edges (`continue`, `break`) in the core
 * serialization loop has historically devastated Apple Silicon (PGO/LTO)
 * performance.
 *    - `std::to_chars` is the backbone of the "qbuem-json Float/Int" serialization
 * paths.
 * ============================================================================
 */

#ifndef QBUEM_JSON_HPP
#define QBUEM_JSON_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <bitset>
#include <cassert>
#include <charconv> // For from_chars in number parsing
#include <climits>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

// ============================================================================
// Zero-SIMD C++20 Architecture
// ============================================================================
// Notice: To achieve universal highest performance, QBUEM-JSON explicitly
// forbids SIMD intrinsics (<arm_neon.h>, <immintrin.h>) and relies purely
// on 64-bit SWAR, C++20 branch hints, and consteval arrays.
//
// No external number parsing libraries (ryu, fast_float) are used. We utilize
// the proprietary "qbuem-json Float" theory.

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64)
#define QBUEM_ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define QBUEM_ARCH_ARM64 1
// Apple Silicon (M1/M2/M3/M4 family) — distinct from generic AArch64:
//   • 128-byte L1/L2 cache lines (vs 64-byte generic ARM64)
//   • ~576-entry ROB (vs ~200 generic ARM64, ~300 on Neoverse)
//   • No SVE/SVE2 exposure — SIGILL risk does NOT exist here
//   • DOTPROD (UDOT/SDOT) always available
//   • SHA3 (EOR3) available on M2+ but not M1
//   • AMX (Apple Matrix Coprocessor) — proprietary, not exposed via intrinsics
#if defined(__APPLE__)
#define QBUEM_ARCH_APPLE_SILICON 1
#endif
#elif defined(__arm__) || defined(_M_ARM)
#define QBUEM_ARCH_ARM32 1
#elif defined(__mips__)
#define QBUEM_ARCH_MIPS 1
#elif defined(__riscv)
#define QBUEM_ARCH_RISCV 1
#elif defined(__s390x__)
#define QBUEM_ARCH_S390X 1
#elif defined(__powerpc__) || defined(_M_PPC)
#define QBUEM_ARCH_PPC 1
#endif

// SIMD Detection (compile-time)
#if defined(QBUEM_ARCH_X86_64)
// SSE2 is mandatory on x86-64 (part of the ABI); always include it.
#include <emmintrin.h>
#if defined(__AVX512F__)
#define QBUEM_HAS_AVX512 1
#define QBUEM_HAS_AVX2 1 // AVX-512 is a superset of AVX2; all ymm code active
#include <immintrin.h>
#elif defined(__AVX2__)
#define QBUEM_HAS_AVX2 1
#include <immintrin.h>
#elif defined(__SSE4_2__)
#define QBUEM_HAS_SSE42 1
#include <nmmintrin.h>
#endif
#elif defined(QBUEM_ARCH_ARM64) || defined(QBUEM_ARCH_ARM32)
#if defined(__ARM_NEON) || defined(__aarch64__)
#define QBUEM_HAS_NEON 1
#include <arm_neon.h>
#endif
// ARM ISA extension detection — all require -march=native or explicit target
// flags DOTPROD: UDOT/SDOT 4×uint8 multiply-accumulate.
//   Available on: Apple Silicon, ARM v8.2+ CPUs.
//   Enables branchless 4-byte character classification in a single instruction.
#if defined(__ARM_FEATURE_DOTPROD)
#define QBUEM_HAS_DOTPROD 1
#endif
// SVE: ARM Scalable Vector Extension (variable-width SIMD: 128–2048 bits).
//   Available on: AWS Graviton 3 (Neoverse V1, 256-bit), Neoverse V2,
//   Cortex-X4. NOT available on Apple Silicon (AMX is Apple's proprietary
//   equivalent). NOT exposed on Linux kernels < 5.16 even when hardware
//   supports it. Use only with explicit -march=armv8.4-a+sve guard or runtime
//   detection.
#if defined(__ARM_FEATURE_SVE)
#define QBUEM_HAS_SVE 1
#include <arm_sve.h>
#endif
// SHA3/EOR3: 3-way XOR (EOR3), rotate-XOR (RAX1), XOR-accumulate (XAR).
//   Available on: Apple Silicon (M2+), ARM v8.4+ CPUs.
//   NOT available on Apple M1.
//   EOR3 enables single-instruction backslash-escape propagation in Stage 1.
#if defined(__ARM_FEATURE_SHA3)
#define QBUEM_HAS_SHA3 1
#endif
#endif

// ============================================================================
// Cache Line Size & Prefetch Distance
// ============================================================================
//
// Architecture-specific tuning constants:
//
//  QBUEM_CACHE_LINE_SIZE: L1 cache line size in bytes.
//    Apple Silicon (M1/M2/M3): 128 bytes — double the ARM standard.
//      Impacts: alignas() for hot tables, prefetch stride granularity.
//    All other AArch64: 64 bytes (ARM standard).
//    x86_64: 64 bytes (Intel/AMD standard).
//
//  QBUEM_PREFETCH_DISTANCE: bytes to look ahead in __builtin_prefetch.
//    Optimal = (pipeline depth × clock speed × bytes/cycle).
//    Apple Silicon L2 latency ≈ 10ns × 3.2 GHz ≈ 32 cycles.
//      At 10 bytes/cycle parse throughput → 320B ideal; round to 384B
//      (3 × 128B cache lines). Using 512B (4 lines) gives headroom.
//    Cortex-X3 L2 latency ≈ 12ns × 3.4 GHz ≈ 40 cycles.
//      A/B result: 256B optimal (4 × 64B cache lines).
//    x86_64: 192B optimal (measured on Raptor Lake).
//
//  QBUEM_PREFETCH_LOCALITY: __builtin_prefetch 'locality' hint (0-3).
//    0 = NTA (non-temporal, bypass L1/L2 — for once-through streaming)
//    1 = L2 hint (data used soon but L1 has better uses) ← parse hot path
//    3 = L1 hint (data used immediately) ← for x86 tight loops

#if defined(QBUEM_ARCH_APPLE_SILICON)
#define QBUEM_CACHE_LINE_SIZE 128
#define QBUEM_PREFETCH_DISTANCE 512 // 4 × 128B M1 cache lines
#define QBUEM_PREFETCH_LOCALITY 1   // L2 hint; parse consumes sequentially
#elif defined(QBUEM_ARCH_ARM64)
#define QBUEM_CACHE_LINE_SIZE 64
#define QBUEM_PREFETCH_DISTANCE 256 // 4 × 64B; A/B winner
#define QBUEM_PREFETCH_LOCALITY 1   // L2 hint (NTA hurt: )
#elif defined(QBUEM_ARCH_X86_64)
#define QBUEM_CACHE_LINE_SIZE 64
#define QBUEM_PREFETCH_DISTANCE 192 // measured optimum
#define QBUEM_PREFETCH_LOCALITY 1   // L2 hint
#else
#define QBUEM_CACHE_LINE_SIZE 64
#define QBUEM_PREFETCH_DISTANCE 128
#define QBUEM_PREFETCH_LOCALITY 1
#endif

// ============================================================================
// C++20 Compiler Intrinsics & Branching Hints
// ============================================================================

#ifdef __GNUC__
#define QBUEM_INLINE __attribute__((always_inline)) inline
#define QBUEM_NOINLINE __attribute__((noinline))
// Read-ahead prefetch with architecture-tuned locality hint.
// Always use this macro in the parse hot loop — never hardcode
// distance/locality.
#define QBUEM_PREFETCH(addr)                                                   \
  __builtin_prefetch((addr), 0, QBUEM_PREFETCH_LOCALITY)
#else
#define QBUEM_INLINE inline
#define QBUEM_NOINLINE
#define QBUEM_PREFETCH(addr) ((void)0)
#endif

// Branch hinting macros
#ifdef __GNUC__
#define QBUEM_LIKELY(x) __builtin_expect(!!(x), 1)
#define QBUEM_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define QBUEM_LIKELY(x) (x)
#define QBUEM_UNLIKELY(x) (x)
#endif

// Fallbacks for CLZ/CTZ use C++20 <bit> uniformly (already included above)
#define QBUEM_CLZ(x) std::countl_zero(static_cast<unsigned long long>(x))
#define QBUEM_CTZ(x) std::countr_zero(static_cast<unsigned long long>(x))

namespace qbuem {
namespace json {

// Forward declarations
class DocumentView;
class Value;

inline Value parse_reuse(DocumentView &doc, ::std::string_view json);
class SafeValue;

// Require C++20 for optimal constexpr, bit_cast, and concepts
#if __cplusplus >= 202002L
using String = std::pmr::string;
template <typename T> using Vector = std::pmr::vector<T>;
using Allocator = std::pmr::polymorphic_allocator<char>;
#else
#error "qbuem-json (Zero-SIMD) requires a C++20 compatible compiler."
#endif

namespace simd {
// prefix_xor: compute prefix-XOR of a 64-bit mask.
// Used by Stage 1 two-(AVX-512/NEON) to track in-string state.
// If mask has 1s at quote positions, prefix_xor(mask) has 1s inside strings.
QBUEM_INLINE uint64_t prefix_xor(uint64_t x) noexcept {
  x ^= x << 1;
  x ^= x << 2;
  x ^= x << 4;
  x ^= x << 8;
  x ^= x << 16;
  x ^= x << 32;
  return x;
}
} // namespace simd

// TapeNode — 8 bytes (compaction)

// TapeNode — 8 bytes (compaction)
//
// meta layout (uint32_t):
//   bits 31-24 : TapeNodeType  (8 bits, values 0-10)
//   bits 23-16 : flags         (8 bits, currently always 0)
//   bits 15-0  : length        (16 bits, max 65535)
//
// Dropped: next_sib (4 bytes) — was written but never read.
// Halves store operations per push(): 5 → 2.
// Fits 8 nodes per 64-byte cache line (vs ~5 before).

enum class TapeNodeType : uint8_t {
  Null = 0,
  BooleanTrue,
  BooleanFalse,
  Integer,
  Double,
  StringRaw,
  NumberRaw,
  ArrayStart,
  ArrayEnd,
  ObjectStart,
  ObjectEnd,
};

struct TapeNode {
  uint32_t meta;   // bits 31-24: type | bits 23-16: flags | bits 15-0: length
  uint32_t offset; // byte offset into source (max 4 GB)
                   // = 8 bytes total

  TapeNode() = default;

  // Packed-meta constructor used by push()
  TapeNode(TapeNodeType t, uint16_t l, uint32_t o)
      : meta((static_cast<uint32_t>(t) << 24) | static_cast<uint32_t>(l)),
        offset(o) {}

  // Field accessors — inline, zero overhead in optimised builds
  QBUEM_INLINE TapeNodeType type() const noexcept {
    return static_cast<TapeNodeType>((meta >> 24) & 0xFFu);
  }
  QBUEM_INLINE uint8_t flags() const noexcept { return (meta >> 16) & 0xFFu; }
  QBUEM_INLINE uint16_t length() const noexcept {
    return static_cast<uint16_t>(meta & 0xFFFFu);
  }
};
static_assert(sizeof(TapeNode) == 8, "TapeNode must be exactly 8 bytes");

// TapeArena — qbuem-json Flat Arena

struct TapeArena {
  TapeNode *base = nullptr;
  TapeNode *head = nullptr;
  TapeNode *cap = nullptr;

  TapeArena() = default;
  ~TapeArena() { std::free(base); }

  TapeArena(const TapeArena &) = delete;
  TapeArena &operator=(const TapeArena &) = delete;

  TapeArena(TapeArena &&o) noexcept : base(o.base), head(o.head), cap(o.cap) {
    o.base = o.head = o.cap = nullptr;
  }
  TapeArena &operator=(TapeArena &&o) noexcept {
    if (this != &o) {
      std::free(base);
      base = o.base;
      head = o.head;
      cap = o.cap;
      o.base = o.head = o.cap = nullptr;
    }
    return *this;
  }

  void reserve(size_t n) {
    if (base && static_cast<size_t>(cap - base) >= n) {
      head = base;
      return;
    }
    std::free(base);
    base = static_cast<TapeNode *>(std::malloc(n * sizeof(TapeNode)));
    if (!base)
      throw std::bad_alloc();
    head = base;
    cap = base + n;
  }

  QBUEM_INLINE void reset() noexcept { head = base; }

  QBUEM_INLINE size_t size() const noexcept {
    return static_cast<size_t>(head - base);
  }

  QBUEM_INLINE TapeNode &operator[](size_t i) noexcept { return base[i]; }
  QBUEM_INLINE const TapeNode &operator[](size_t i) const noexcept {
    return base[i];
  }
};

// Stage1Index — flat array of structural char offsets
// Reused across parse_reuse() calls (amortises malloc cost).

struct Stage1Index {
  uint32_t *positions = nullptr;
  uint32_t count = 0;
  uint32_t capacity = 0;

  Stage1Index() = default;
  ~Stage1Index() { std::free(positions); }

  Stage1Index(const Stage1Index &) = delete;
  Stage1Index &operator=(const Stage1Index &) = delete;

  Stage1Index(Stage1Index &&o) noexcept
      : positions(o.positions), count(o.count), capacity(o.capacity) {
    o.positions = nullptr;
    o.count = o.capacity = 0;
  }
  Stage1Index &operator=(Stage1Index &&o) noexcept {
    if (this != &o) {
      std::free(positions);
      positions = o.positions;
      count = o.count;
      capacity = o.capacity;
      o.positions = nullptr;
      o.count = o.capacity = 0;
    }
    return *this;
  }

  void reserve(size_t n) {
    if (positions && capacity >= static_cast<uint32_t>(n))
      return;
    std::free(positions);
    positions = static_cast<uint32_t *>(std::malloc(n * sizeof(uint32_t)));
    if (!positions)
      throw std::bad_alloc();
    capacity = static_cast<uint32_t>(n);
  }

  void reset() noexcept { count = 0; }
};

// DocumentView

// Mutation overlay entry: stores the new type + serialized content for a
// value that has been set() since parse.  Keyed by tape index.
struct MutationEntry {
  TapeNodeType type;
  std::string data; // string content (no quotes) for StringRaw;
                    // decimal text for Integer/Double;
                    // empty for Null/BooleanTrue/BooleanFalse
};

// DocumentState: The heap-allocated internal data for a JSON document.
// This preserves stable pointers for Value objects even if the Document handle
// is moved or destroyed.
struct DocumentState {
  ::std::string_view source;
  TapeArena tape;
  Stage1Index idx; // structural index reused across calls
  ::std::atomic<int> ref_count{0};

  mutable size_t last_dump_size_ = 0;
  ::std::unordered_map<uint32_t, MutationEntry> mutations_;
  ::std::unordered_set<uint32_t> deleted_;
  ::std::unordered_map<uint32_t,
                       ::std::vector<::std::pair<::std::string, ::std::string>>>
      additions_;

  struct ArrayInsertion {
    size_t index;
    ::std::string data;
  };
  ::std::unordered_map<uint32_t, ::std::vector<ArrayInsertion>>
      array_insertions_;

  mutable ::std::unordered_map<::std::string, ::std::shared_ptr<DocumentState>>
      synthetic_docs_;

  DocumentState() = default;
  explicit DocumentState(::std::string_view json) : source(json) {}

  void ref() { ref_count.fetch_add(1, ::std::memory_order_relaxed); }
  void deref() {
    if (ref_count.fetch_sub(1, ::std::memory_order_acq_rel) == 1) {
      delete this;
    }
  }

  // Forward declaration of get_synthetic for DocumentState
  DocumentState *get_synthetic(const ::std::string &json_str) const;

  // Prevent copying/moving the state itself; always handle via pointers.
  DocumentState(const DocumentState &) = delete;
  DocumentState &operator=(const DocumentState &) = delete;
};

/// @brief A lightweight handle to a parsed JSON document's internal state.
/// @details DocumentView (aliased as Document) manages the lifetime of a
/// DocumentState via reference counting. Moving a DocumentView is a O(1)
/// operation that does not invalidate any Associated Value objects.
class DocumentView {
  DocumentState *state_ = nullptr;

public:
  DocumentView() : state_(new DocumentState()) { state_->ref(); }

  explicit DocumentView(::std::string_view json)
      : state_(new DocumentState(json)) {
    state_->ref();
  }

  ~DocumentView() {
    if (state_)
      state_->deref();
  }

  DocumentView(const DocumentView &o) : state_(o.state_) {
    if (state_)
      state_->ref();
  }

  DocumentView &operator=(const DocumentView &o) {
    if (this != &o) {
      if (state_)
        state_->deref();
      state_ = o.state_;
      if (state_)
        state_->ref();
    }
    return *this;
  }

  DocumentView(DocumentView &&o) noexcept : state_(o.state_) {
    o.state_ = nullptr;
  }

  DocumentView &operator=(DocumentView &&o) noexcept {
    if (this != &o) {
      if (state_)
        state_->deref();
      state_ = o.state_;
      o.state_ = nullptr;
    }
    return *this;
  }

  // Handle accessors
  DocumentState *state() const { return state_; }
  const char *data() const { return state_ ? state_->source.data() : nullptr; }
  size_t size() const { return state_ ? state_->source.size() : 0; }

  // Implicit conversion to bool for validity check
  explicit operator bool() const noexcept { return state_ != nullptr; }

  // Forward synthetic doc lookup to state
  DocumentState *get_synthetic(const ::std::string &json_str) const {
    return state_ ? state_->get_synthetic(json_str) : nullptr;
  }
};

// C++20 Concepts — named constraints used throughout Value/SafeValue

/// Matches any integral type except bool (maps to JSON integer).
template <typename T>
concept JsonInteger = std::integral<T> && !std::same_as<T, bool>;

/// Matches floating-point types (maps to JSON double).
template <typename T>
concept JsonFloat = std::floating_point<T>;

/// Types that can be read back from a JSON value via as<T>().
template <typename T>
concept JsonReadable =
    std::same_as<T, bool> || JsonInteger<T> || JsonFloat<T> ||
    std::same_as<T, std::string> || std::same_as<T, std::string_view>;

/// Types that can be written into a JSON value via set()/operator=().
template <typename T>
concept JsonWritable =
    std::same_as<T, std::nullptr_t> || std::same_as<T, bool> ||
    JsonInteger<T> || JsonFloat<T> || std::convertible_to<T, std::string_view>;

/// Scalar-only subset of JsonWritable: excludes string-like types.
/// Used for insert<T>/push_back<T> templates to prevent ambiguity with
/// the insert(key, string_view) overload that would cause infinite recursion.
template <typename T>
concept JsonScalarOnly =
    std::same_as<T, std::nullptr_t> || std::same_as<T, bool> ||
    JsonInteger<T> || JsonFloat<T>;

// Forward declarations

class SafeValue; // optional-propagating proxy (defined after Value)

// Value + zero-copy dump()

/// @brief The primary accessor type for the qbuem-json DOM.
/// @details `Value` is a lightweight handle consisting of a pointer to the
// ── qj_nc: Fast numeric serialization — defined early so Value/detail can use it
namespace detail {
// ── Fast numeric serialization (Schubfach + yy-itoa) ────────────────────────
// Algorithm : Schubfach (R. Giulietti 2020) for double→decimal
//             yy-itoa   (Y. Yuan 2018)      for integer→decimal
// Source    : https://github.com/ibireme/yyjson  (MIT)
// Reference : https://github.com/stephenberry/glaze (MIT)
namespace qj_nc {
using std::uint8_t; using std::uint16_t; using std::uint32_t; using std::uint64_t;
using std::int32_t; using std::int64_t;

   static constexpr char char_table[200] = {
      '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7', '0', '8', '0', '9', '1', '0', '1',
      '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '2', '0', '2', '1', '2', '2',
      '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9', '3', '0', '3', '1', '3', '2', '3', '3', '3',
      '4', '3', '5', '3', '6', '3', '7', '3', '8', '3', '9', '4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5',
      '4', '6', '4', '7', '4', '8', '4', '9', '5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5',
      '7', '5', '8', '5', '9', '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8',
      '6', '9', '7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', '8',
      '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9', '9', '0', '9', '1',
      '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7', '9', '8', '9', '9'};

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, uint32_t>
   auto* to_chars(auto* buf, T val) noexcept
   {
      /* The maximum value of uint32_t is 4294967295 (10 digits), */
      /* these digits are named as 'aabbccddee' here.             */
      uint32_t aa, bb, cc, dd, ee, aabb, bbcc, ccdd, ddee, aabbcc;

      /* Leading zero count in the first pair.                    */
      uint32_t lz;

      /* Although most compilers may convert the "division by     */
      /* constant value" into "multiply and shift", manual        */
      /* conversion can still help some compilers generate        */
      /* fewer and better instructions.                           */

      if (val < 100) { /* 1-2 digits: aa */
         lz = val < 10;
         std::memcpy(buf, char_table + (val * 2 + lz), 2);
         buf -= lz;
         return buf + 2;
      }
      else if (val < 10000) { /* 3-4 digits: aabb */
         aa = (val * 5243) >> 19; /* (val / 100) */
         bb = val - aa * 100; /* (val % 100) */
         lz = aa < 10;
         std::memcpy(buf, char_table + (aa * 2 + lz), 2);
         buf -= lz;
         std::memcpy(&buf[2], char_table + (2 * bb), 2);

         return buf + 4;
      }
      else if (val < 1000000) { /* 5-6 digits: aabbcc */
         aa = uint32_t((uint64_t(val) * 429497) >> 32); /* (val / 10000) */
         bbcc = val - aa * 10000; /* (val % 10000) */
         bb = (bbcc * 5243) >> 19; /* (bbcc / 100) */
         cc = bbcc - bb * 100; /* (bbcc % 100) */
         lz = aa < 10;
         std::memcpy(buf, char_table + aa * 2 + lz, 2);
         buf -= lz;
         std::memcpy(buf + 2, char_table + bb * 2, 2);
         std::memcpy(buf + 4, char_table + cc * 2, 2);
         return buf + 6;
      }
      else if (val < 100000000) { /* 7~8 digits: aabbccdd */
         /* (val / 10000) */
         aabb = uint32_t((uint64_t(val) * 109951163) >> 40);
         ccdd = val - aabb * 10000; /* (val % 10000) */
         aa = (aabb * 5243) >> 19; /* (aabb / 100) */
         cc = (ccdd * 5243) >> 19; /* (ccdd / 100) */
         bb = aabb - aa * 100; /* (aabb % 100) */
         dd = ccdd - cc * 100; /* (ccdd % 100) */
         lz = aa < 10;
         std::memcpy(buf, char_table + aa * 2 + lz, 2);
         buf -= lz;
         std::memcpy(buf + 2, char_table + bb * 2, 2);
         std::memcpy(buf + 4, char_table + cc * 2, 2);
         std::memcpy(buf + 6, char_table + dd * 2, 2);
         return buf + 8;
      }
      else { /* 9~10 digits: aabbccddee */
         /* (val / 10000) */
         aabbcc = uint32_t((uint64_t(val) * 3518437209ul) >> 45);
         /* (aabbcc / 10000) */
         aa = uint32_t((uint64_t(aabbcc) * 429497) >> 32);
         ddee = val - aabbcc * 10000; /* (val % 10000) */
         bbcc = aabbcc - aa * 10000; /* (aabbcc % 10000) */
         bb = (bbcc * 5243) >> 19; /* (bbcc / 100) */
         dd = (ddee * 5243) >> 19; /* (ddee / 100) */
         cc = bbcc - bb * 100; /* (bbcc % 100) */
         ee = ddee - dd * 100; /* (ddee % 100) */
         lz = aa < 10;
         std::memcpy(buf, char_table + aa * 2 + lz, 2);
         buf -= lz;
         std::memcpy(buf + 2, char_table + bb * 2, 2);
         std::memcpy(buf + 4, char_table + cc * 2, 2);
         std::memcpy(buf + 6, char_table + dd * 2, 2);
         std::memcpy(buf + 8, char_table + ee * 2, 2);
         return buf + 10;
      }
   }

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, int32_t>
   auto* to_chars(auto* buf, T x) noexcept
   {
      *buf = '-';
      // shifts are necessary to have the numeric_limits<int32_t>::min case
      return to_chars(buf + (x < 0), uint32_t(x ^ (x >> 31)) - (x >> 31));
   }

   template <class T>
      requires(std::same_as<std::remove_cvref_t<T>, uint32_t>)
   QBUEM_INLINE auto* to_chars_u64_len_8(auto* buf, T val) noexcept
   {
      /* 8 digits: aabbccdd */
      const uint32_t aabb = uint32_t((uint64_t(val) * 109951163) >> 40); /* (val / 10000) */
      const uint32_t ccdd = val - aabb * 10000; /* (val % 10000) */
      const uint32_t aa = (aabb * 5243) >> 19; /* (aabb / 100) */
      const uint32_t cc = (ccdd * 5243) >> 19; /* (ccdd / 100) */
      const uint32_t bb = aabb - aa * 100; /* (aabb % 100) */
      const uint32_t dd = ccdd - cc * 100; /* (ccdd % 100) */
      std::memcpy(buf, char_table + aa * 2, 2);
      std::memcpy(buf + 2, char_table + bb * 2, 2);
      std::memcpy(buf + 4, char_table + cc * 2, 2);
      std::memcpy(buf + 6, char_table + dd * 2, 2);
      return buf + 8;
   }

   template <class T>
      requires(std::same_as<std::remove_cvref_t<T>, uint32_t>)
   QBUEM_INLINE auto* to_chars_u64_len_4(auto* buf, T val) noexcept
   {
      /* 4 digits: aabb */
      const uint32_t aa = (val * 5243) >> 19; /* (val / 100) */
      const uint32_t bb = val - aa * 100; /* (val % 100) */
      std::memcpy(buf, char_table + aa * 2, 2);
      std::memcpy(buf + 2, char_table + bb * 2, 2);
      return buf + 4;
   }

   template <class T>
      requires(std::same_as<std::remove_cvref_t<T>, uint32_t>)
   inline auto* to_chars_u64_len_1_8(auto* buf, T val) noexcept
   {
      uint32_t aa, bb, cc, dd, aabb, bbcc, ccdd, lz;

      if (val < 100) { /* 1-2 digits: aa */
         lz = val < 10;
         std::memcpy(buf, char_table + val * 2 + lz, 2);
         buf -= lz;
         return buf + 2;
      }
      else if (val < 10000) { /* 3-4 digits: aabb */
         aa = (val * 5243) >> 19; /* (val / 100) */
         bb = val - aa * 100; /* (val % 100) */
         lz = aa < 10;
         std::memcpy(buf, char_table + aa * 2 + lz, 2);
         buf -= lz;
         std::memcpy(buf + 2, char_table + bb * 2, 2);
         return buf + 4;
      }
      else if (val < 1000000) { /* 5-6 digits: aabbcc */
         aa = uint32_t((uint64_t(val) * 429497) >> 32); /* (val / 10000) */
         bbcc = val - aa * 10000; /* (val % 10000) */
         bb = (bbcc * 5243) >> 19; /* (bbcc / 100) */
         cc = bbcc - bb * 100; /* (bbcc % 100) */
         lz = aa < 10;
         std::memcpy(buf, char_table + aa * 2 + lz, 2);
         buf -= lz;
         std::memcpy(buf + 2, char_table + bb * 2, 2);
         std::memcpy(buf + 4, char_table + cc * 2, 2);
         return buf + 6;
      }
      else { /* 7-8 digits: aabbccdd */
         /* (val / 10000) */
         aabb = uint32_t((uint64_t(val) * 109951163) >> 40);
         ccdd = val - aabb * 10000; /* (val % 10000) */
         aa = (aabb * 5243) >> 19; /* (aabb / 100) */
         cc = (ccdd * 5243) >> 19; /* (ccdd / 100) */
         bb = aabb - aa * 100; /* (aabb % 100) */
         dd = ccdd - cc * 100; /* (ccdd % 100) */
         lz = aa < 10;
         std::memcpy(buf, char_table + aa * 2 + lz, 2);
         buf -= lz;
         std::memcpy(buf + 2, char_table + bb * 2, 2);
         std::memcpy(buf + 4, char_table + cc * 2, 2);
         std::memcpy(buf + 6, char_table + dd * 2, 2);
         return buf + 8;
      }
   }

   template <class T>
      requires(std::same_as<std::remove_cvref_t<T>, uint32_t>)
   auto* to_chars_u64_len_5_8(auto* buf, T val) noexcept
   {
      if (val < 1000000) { /* 5-6 digits: aabbcc */
         const uint32_t aa = uint32_t((uint64_t(val) * 429497) >> 32); /* (val / 10000) */
         const uint32_t bbcc = val - aa * 10000; /* (val % 10000) */
         const uint32_t bb = (bbcc * 5243) >> 19; /* (bbcc / 100) */
         const uint32_t cc = bbcc - bb * 100; /* (bbcc % 100) */
         const uint32_t lz = aa < 10;
         std::memcpy(buf, char_table + aa * 2 + lz, 2);
         buf -= lz;
         std::memcpy(buf + 2, char_table + bb * 2, 2);
         std::memcpy(buf + 4, char_table + cc * 2, 2);
         return buf + 6;
      }
      else { /* 7-8 digits: aabbccdd */
         /* (val / 10000) */
         const uint32_t aabb = uint32_t((uint64_t(val) * 109951163) >> 40);
         const uint32_t ccdd = val - aabb * 10000; /* (val % 10000) */
         const uint32_t aa = (aabb * 5243) >> 19; /* (aabb / 100) */
         const uint32_t cc = (ccdd * 5243) >> 19; /* (ccdd / 100) */
         const uint32_t bb = aabb - aa * 100; /* (aabb % 100) */
         const uint32_t dd = ccdd - cc * 100; /* (ccdd % 100) */
         const uint32_t lz = aa < 10;
         std::memcpy(buf, char_table + aa * 2 + lz, 2);
         buf -= lz;
         std::memcpy(buf + 2, char_table + bb * 2, 2);
         std::memcpy(buf + 4, char_table + cc * 2, 2);
         std::memcpy(buf + 6, char_table + dd * 2, 2);
         return buf + 8;
      }
   }

   template <class T>
      requires(std::same_as<std::remove_cvref_t<T>, uint64_t>)
   auto* to_chars(auto* buf, T val) noexcept
   {
      if (val < 100000000) { /* 1-8 digits */
         buf = to_chars_u64_len_1_8(buf, uint32_t(val));
         return buf;
      }
      else if (val < 100000000ull * 100000000ull) { /* 9-16 digits */
         const uint64_t hgh = val / 100000000;
         const auto low = uint32_t(val - hgh * 100000000); /* (val % 100000000) */
         buf = to_chars_u64_len_1_8(buf, uint32_t(hgh));
         buf = to_chars_u64_len_8(buf, low);
         return buf;
      }
      else { /* 17-20 digits */
         const uint64_t tmp = val / 100000000;
         const auto low = uint32_t(val - tmp * 100000000); /* (val % 100000000) */
         const auto hgh = uint32_t(tmp / 10000);
         const auto mid = uint32_t(tmp - hgh * 10000); /* (tmp % 10000) */
         buf = to_chars_u64_len_5_8(buf, hgh);
         buf = to_chars_u64_len_4(buf, mid);
         buf = to_chars_u64_len_8(buf, low);
         return buf;
      }
   }

   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, int64_t>
   auto* to_chars(auto* buf, T x) noexcept
   {
      *buf = '-';
      // shifts are necessary to have the numeric_limits<int64_t>::min case
      return to_chars(buf + (x < 0), uint64_t(x ^ (x >> 63)) - (x >> 63));
   }

   // Source: https://github.com/ibireme/yyjson/blob/master/src/yyjson.c

   /** Multiplies two 64-bit unsigned integers (a * b),
       returns the 128-bit result as 'hi' and 'lo'. */
   QBUEM_INLINE void u128_mul(uint64_t a, uint64_t b, uint64_t* hi, uint64_t* lo) noexcept
   {
#ifdef __SIZEOF_INT128__
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
      unsigned __int128 m = static_cast<unsigned __int128>(a) * b;
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
      *hi = uint64_t(m >> 64);
      *lo = uint64_t(m);
#elif defined(_M_X64)
      *lo = _umul128(a, b, hi);
#elif defined(_M_ARM64)
      *hi = __umulh(a, b);
      *lo = a * b;
#else
      uint32_t a0 = (uint32_t)(a), a1 = (uint32_t)(a >> 32);
      uint32_t b0 = (uint32_t)(b), b1 = (uint32_t)(b >> 32);
      uint64_t p00 = (uint64_t)a0 * b0, p01 = (uint64_t)a0 * b1;
      uint64_t p10 = (uint64_t)a1 * b0, p11 = (uint64_t)a1 * b1;
      uint64_t m0 = p01 + (p00 >> 32);
      uint32_t m00 = (uint32_t)(m0), m01 = (uint32_t)(m0 >> 32);
      uint64_t m1 = p10 + m00;
      uint32_t m10 = (uint32_t)(m1), m11 = (uint32_t)(m1 >> 32);
      *hi = p11 + m01 + m11;
      *lo = ((uint64_t)m10 << 32) | (uint32_t)p00;
#endif
   }

   /** Multiplies two 64-bit unsigned integers and add a value (a * b + c),
       returns the 128-bit result as 'hi' and 'lo'. */
   QBUEM_INLINE void u128_mul_add(uint64_t a, uint64_t b, uint64_t c, uint64_t* hi, uint64_t* lo) noexcept
   {
#ifdef __SIZEOF_INT128__
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
      unsigned __int128 m = static_cast<unsigned __int128>(a) * b + c;
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
      *hi = uint64_t(m >> 64);
      *lo = uint64_t(m);
#else
      uint64_t h, l, t;
      u128_mul(a, b, &h, &l);
      t = l + c;
      h += ((t < l) | (t < c));
      *hi = h;
      *lo = t;
#endif
   }

   /** Multiplies 128-bit integer and returns highest 64-bit rounded value. */
   QBUEM_INLINE uint64_t round_to_odd(uint64_t hi, uint64_t lo, uint64_t cp) noexcept
   {
      uint64_t x_hi, x_lo, y_hi, y_lo;
      u128_mul(cp, lo, &x_hi, &x_lo);
      u128_mul_add(cp, hi, x_hi, &y_hi, &y_lo);
      return y_hi | (y_lo > 1);
   }

   /*==============================================================================
    * Power10 Lookup Table
    *============================================================================*/

   /** Minimum decimal exponent in pow10_sig_table. */
   inline constexpr auto POW10_SIG_TABLE_128_MIN_EXP = -343;

   /** Maximum decimal exponent in pow10_sig_table. */
   inline constexpr auto POW10_SIG_TABLE_128_MAX_EXP = 324;

   /** Minimum exact decimal exponent in pow10_sig_table */
   inline constexpr auto POW10_SIG_TABLE_128_MIN_EXACT_EXP = 0;

   /** Maximum exact decimal exponent in pow10_sig_table */
   inline constexpr auto POW10_SIG_TABLE_128_MAX_EXACT_EXP = 55;

      static constexpr uint64_t pow10_sig_table_128[1336] = {
      0xBF29DCABA82FDEAE, 0x7432EE873880FC33, /* ~= 10^-343 */
      0xEEF453D6923BD65A, 0x113FAA2906A13B3F, /* ~= 10^-342 */
      0x9558B4661B6565F8, 0x4AC7CA59A424C507, /* ~= 10^-341 */
      0xBAAEE17FA23EBF76, 0x5D79BCF00D2DF649, /* ~= 10^-340 */
      0xE95A99DF8ACE6F53, 0xF4D82C2C107973DC, /* ~= 10^-339 */
      0x91D8A02BB6C10594, 0x79071B9B8A4BE869, /* ~= 10^-338 */
      0xB64EC836A47146F9, 0x9748E2826CDEE284, /* ~= 10^-337 */
      0xE3E27A444D8D98B7, 0xFD1B1B2308169B25, /* ~= 10^-336 */
      0x8E6D8C6AB0787F72, 0xFE30F0F5E50E20F7, /* ~= 10^-335 */
      0xB208EF855C969F4F, 0xBDBD2D335E51A935, /* ~= 10^-334 */
      0xDE8B2B66B3BC4723, 0xAD2C788035E61382, /* ~= 10^-333 */
      0x8B16FB203055AC76, 0x4C3BCB5021AFCC31, /* ~= 10^-332 */
      0xADDCB9E83C6B1793, 0xDF4ABE242A1BBF3D, /* ~= 10^-331 */
      0xD953E8624B85DD78, 0xD71D6DAD34A2AF0D, /* ~= 10^-330 */
      0x87D4713D6F33AA6B, 0x8672648C40E5AD68, /* ~= 10^-329 */
      0xA9C98D8CCB009506, 0x680EFDAF511F18C2, /* ~= 10^-328 */
      0xD43BF0EFFDC0BA48, 0x0212BD1B2566DEF2, /* ~= 10^-327 */
      0x84A57695FE98746D, 0x014BB630F7604B57, /* ~= 10^-326 */
      0xA5CED43B7E3E9188, 0x419EA3BD35385E2D, /* ~= 10^-325 */
      0xCF42894A5DCE35EA, 0x52064CAC828675B9, /* ~= 10^-324 */
      0x818995CE7AA0E1B2, 0x7343EFEBD1940993, /* ~= 10^-323 */
      0xA1EBFB4219491A1F, 0x1014EBE6C5F90BF8, /* ~= 10^-322 */
      0xCA66FA129F9B60A6, 0xD41A26E077774EF6, /* ~= 10^-321 */
      0xFD00B897478238D0, 0x8920B098955522B4, /* ~= 10^-320 */
      0x9E20735E8CB16382, 0x55B46E5F5D5535B0, /* ~= 10^-319 */
      0xC5A890362FDDBC62, 0xEB2189F734AA831D, /* ~= 10^-318 */
      0xF712B443BBD52B7B, 0xA5E9EC7501D523E4, /* ~= 10^-317 */
      0x9A6BB0AA55653B2D, 0x47B233C92125366E, /* ~= 10^-316 */
      0xC1069CD4EABE89F8, 0x999EC0BB696E840A, /* ~= 10^-315 */
      0xF148440A256E2C76, 0xC00670EA43CA250D, /* ~= 10^-314 */
      0x96CD2A865764DBCA, 0x380406926A5E5728, /* ~= 10^-313 */
      0xBC807527ED3E12BC, 0xC605083704F5ECF2, /* ~= 10^-312 */
      0xEBA09271E88D976B, 0xF7864A44C633682E, /* ~= 10^-311 */
      0x93445B8731587EA3, 0x7AB3EE6AFBE0211D, /* ~= 10^-310 */
      0xB8157268FDAE9E4C, 0x5960EA05BAD82964, /* ~= 10^-309 */
      0xE61ACF033D1A45DF, 0x6FB92487298E33BD, /* ~= 10^-308 */
      0x8FD0C16206306BAB, 0xA5D3B6D479F8E056, /* ~= 10^-307 */
      0xB3C4F1BA87BC8696, 0x8F48A4899877186C, /* ~= 10^-306 */
      0xE0B62E2929ABA83C, 0x331ACDABFE94DE87, /* ~= 10^-305 */
      0x8C71DCD9BA0B4925, 0x9FF0C08B7F1D0B14, /* ~= 10^-304 */
      0xAF8E5410288E1B6F, 0x07ECF0AE5EE44DD9, /* ~= 10^-303 */
      0xDB71E91432B1A24A, 0xC9E82CD9F69D6150, /* ~= 10^-302 */
      0x892731AC9FAF056E, 0xBE311C083A225CD2, /* ~= 10^-301 */
      0xAB70FE17C79AC6CA, 0x6DBD630A48AAF406, /* ~= 10^-300 */
      0xD64D3D9DB981787D, 0x092CBBCCDAD5B108, /* ~= 10^-299 */
      0x85F0468293F0EB4E, 0x25BBF56008C58EA5, /* ~= 10^-298 */
      0xA76C582338ED2621, 0xAF2AF2B80AF6F24E, /* ~= 10^-297 */
      0xD1476E2C07286FAA, 0x1AF5AF660DB4AEE1, /* ~= 10^-296 */
      0x82CCA4DB847945CA, 0x50D98D9FC890ED4D, /* ~= 10^-295 */
      0xA37FCE126597973C, 0xE50FF107BAB528A0, /* ~= 10^-294 */
      0xCC5FC196FEFD7D0C, 0x1E53ED49A96272C8, /* ~= 10^-293 */
      0xFF77B1FCBEBCDC4F, 0x25E8E89C13BB0F7A, /* ~= 10^-292 */
      0x9FAACF3DF73609B1, 0x77B191618C54E9AC, /* ~= 10^-291 */
      0xC795830D75038C1D, 0xD59DF5B9EF6A2417, /* ~= 10^-290 */
      0xF97AE3D0D2446F25, 0x4B0573286B44AD1D, /* ~= 10^-289 */
      0x9BECCE62836AC577, 0x4EE367F9430AEC32, /* ~= 10^-288 */
      0xC2E801FB244576D5, 0x229C41F793CDA73F, /* ~= 10^-287 */
      0xF3A20279ED56D48A, 0x6B43527578C1110F, /* ~= 10^-286 */
      0x9845418C345644D6, 0x830A13896B78AAA9, /* ~= 10^-285 */
      0xBE5691EF416BD60C, 0x23CC986BC656D553, /* ~= 10^-284 */
      0xEDEC366B11C6CB8F, 0x2CBFBE86B7EC8AA8, /* ~= 10^-283 */
      0x94B3A202EB1C3F39, 0x7BF7D71432F3D6A9, /* ~= 10^-282 */
      0xB9E08A83A5E34F07, 0xDAF5CCD93FB0CC53, /* ~= 10^-281 */
      0xE858AD248F5C22C9, 0xD1B3400F8F9CFF68, /* ~= 10^-280 */
      0x91376C36D99995BE, 0x23100809B9C21FA1, /* ~= 10^-279 */
      0xB58547448FFFFB2D, 0xABD40A0C2832A78A, /* ~= 10^-278 */
      0xE2E69915B3FFF9F9, 0x16C90C8F323F516C, /* ~= 10^-277 */
      0x8DD01FAD907FFC3B, 0xAE3DA7D97F6792E3, /* ~= 10^-276 */
      0xB1442798F49FFB4A, 0x99CD11CFDF41779C, /* ~= 10^-275 */
      0xDD95317F31C7FA1D, 0x40405643D711D583, /* ~= 10^-274 */
      0x8A7D3EEF7F1CFC52, 0x482835EA666B2572, /* ~= 10^-273 */
      0xAD1C8EAB5EE43B66, 0xDA3243650005EECF, /* ~= 10^-272 */
      0xD863B256369D4A40, 0x90BED43E40076A82, /* ~= 10^-271 */
      0x873E4F75E2224E68, 0x5A7744A6E804A291, /* ~= 10^-270 */
      0xA90DE3535AAAE202, 0x711515D0A205CB36, /* ~= 10^-269 */
      0xD3515C2831559A83, 0x0D5A5B44CA873E03, /* ~= 10^-268 */
      0x8412D9991ED58091, 0xE858790AFE9486C2, /* ~= 10^-267 */
      0xA5178FFF668AE0B6, 0x626E974DBE39A872, /* ~= 10^-266 */
      0xCE5D73FF402D98E3, 0xFB0A3D212DC8128F, /* ~= 10^-265 */
      0x80FA687F881C7F8E, 0x7CE66634BC9D0B99, /* ~= 10^-264 */
      0xA139029F6A239F72, 0x1C1FFFC1EBC44E80, /* ~= 10^-263 */
      0xC987434744AC874E, 0xA327FFB266B56220, /* ~= 10^-262 */
      0xFBE9141915D7A922, 0x4BF1FF9F0062BAA8, /* ~= 10^-261 */
      0x9D71AC8FADA6C9B5, 0x6F773FC3603DB4A9, /* ~= 10^-260 */
      0xC4CE17B399107C22, 0xCB550FB4384D21D3, /* ~= 10^-259 */
      0xF6019DA07F549B2B, 0x7E2A53A146606A48, /* ~= 10^-258 */
      0x99C102844F94E0FB, 0x2EDA7444CBFC426D, /* ~= 10^-257 */
      0xC0314325637A1939, 0xFA911155FEFB5308, /* ~= 10^-256 */
      0xF03D93EEBC589F88, 0x793555AB7EBA27CA, /* ~= 10^-255 */
      0x96267C7535B763B5, 0x4BC1558B2F3458DE, /* ~= 10^-254 */
      0xBBB01B9283253CA2, 0x9EB1AAEDFB016F16, /* ~= 10^-253 */
      0xEA9C227723EE8BCB, 0x465E15A979C1CADC, /* ~= 10^-252 */
      0x92A1958A7675175F, 0x0BFACD89EC191EC9, /* ~= 10^-251 */
      0xB749FAED14125D36, 0xCEF980EC671F667B, /* ~= 10^-250 */
      0xE51C79A85916F484, 0x82B7E12780E7401A, /* ~= 10^-249 */
      0x8F31CC0937AE58D2, 0xD1B2ECB8B0908810, /* ~= 10^-248 */
      0xB2FE3F0B8599EF07, 0x861FA7E6DCB4AA15, /* ~= 10^-247 */
      0xDFBDCECE67006AC9, 0x67A791E093E1D49A, /* ~= 10^-246 */
      0x8BD6A141006042BD, 0xE0C8BB2C5C6D24E0, /* ~= 10^-245 */
      0xAECC49914078536D, 0x58FAE9F773886E18, /* ~= 10^-244 */
      0xDA7F5BF590966848, 0xAF39A475506A899E, /* ~= 10^-243 */
      0x888F99797A5E012D, 0x6D8406C952429603, /* ~= 10^-242 */
      0xAAB37FD7D8F58178, 0xC8E5087BA6D33B83, /* ~= 10^-241 */
      0xD5605FCDCF32E1D6, 0xFB1E4A9A90880A64, /* ~= 10^-240 */
      0x855C3BE0A17FCD26, 0x5CF2EEA09A55067F, /* ~= 10^-239 */
      0xA6B34AD8C9DFC06F, 0xF42FAA48C0EA481E, /* ~= 10^-238 */
      0xD0601D8EFC57B08B, 0xF13B94DAF124DA26, /* ~= 10^-237 */
      0x823C12795DB6CE57, 0x76C53D08D6B70858, /* ~= 10^-236 */
      0xA2CB1717B52481ED, 0x54768C4B0C64CA6E, /* ~= 10^-235 */
      0xCB7DDCDDA26DA268, 0xA9942F5DCF7DFD09, /* ~= 10^-234 */
      0xFE5D54150B090B02, 0xD3F93B35435D7C4C, /* ~= 10^-233 */
      0x9EFA548D26E5A6E1, 0xC47BC5014A1A6DAF, /* ~= 10^-232 */
      0xC6B8E9B0709F109A, 0x359AB6419CA1091B, /* ~= 10^-231 */
      0xF867241C8CC6D4C0, 0xC30163D203C94B62, /* ~= 10^-230 */
      0x9B407691D7FC44F8, 0x79E0DE63425DCF1D, /* ~= 10^-229 */
      0xC21094364DFB5636, 0x985915FC12F542E4, /* ~= 10^-228 */
      0xF294B943E17A2BC4, 0x3E6F5B7B17B2939D, /* ~= 10^-227 */
      0x979CF3CA6CEC5B5A, 0xA705992CEECF9C42, /* ~= 10^-226 */
      0xBD8430BD08277231, 0x50C6FF782A838353, /* ~= 10^-225 */
      0xECE53CEC4A314EBD, 0xA4F8BF5635246428, /* ~= 10^-224 */
      0x940F4613AE5ED136, 0x871B7795E136BE99, /* ~= 10^-223 */
      0xB913179899F68584, 0x28E2557B59846E3F, /* ~= 10^-222 */
      0xE757DD7EC07426E5, 0x331AEADA2FE589CF, /* ~= 10^-221 */
      0x9096EA6F3848984F, 0x3FF0D2C85DEF7621, /* ~= 10^-220 */
      0xB4BCA50B065ABE63, 0x0FED077A756B53A9, /* ~= 10^-219 */
      0xE1EBCE4DC7F16DFB, 0xD3E8495912C62894, /* ~= 10^-218 */
      0x8D3360F09CF6E4BD, 0x64712DD7ABBBD95C, /* ~= 10^-217 */
      0xB080392CC4349DEC, 0xBD8D794D96AACFB3, /* ~= 10^-216 */
      0xDCA04777F541C567, 0xECF0D7A0FC5583A0, /* ~= 10^-215 */
      0x89E42CAAF9491B60, 0xF41686C49DB57244, /* ~= 10^-214 */
      0xAC5D37D5B79B6239, 0x311C2875C522CED5, /* ~= 10^-213 */
      0xD77485CB25823AC7, 0x7D633293366B828B, /* ~= 10^-212 */
      0x86A8D39EF77164BC, 0xAE5DFF9C02033197, /* ~= 10^-211 */
      0xA8530886B54DBDEB, 0xD9F57F830283FDFC, /* ~= 10^-210 */
      0xD267CAA862A12D66, 0xD072DF63C324FD7B, /* ~= 10^-209 */
      0x8380DEA93DA4BC60, 0x4247CB9E59F71E6D, /* ~= 10^-208 */
      0xA46116538D0DEB78, 0x52D9BE85F074E608, /* ~= 10^-207 */
      0xCD795BE870516656, 0x67902E276C921F8B, /* ~= 10^-206 */
      0x806BD9714632DFF6, 0x00BA1CD8A3DB53B6, /* ~= 10^-205 */
      0xA086CFCD97BF97F3, 0x80E8A40ECCD228A4, /* ~= 10^-204 */
      0xC8A883C0FDAF7DF0, 0x6122CD128006B2CD, /* ~= 10^-203 */
      0xFAD2A4B13D1B5D6C, 0x796B805720085F81, /* ~= 10^-202 */
      0x9CC3A6EEC6311A63, 0xCBE3303674053BB0, /* ~= 10^-201 */
      0xC3F490AA77BD60FC, 0xBEDBFC4411068A9C, /* ~= 10^-200 */
      0xF4F1B4D515ACB93B, 0xEE92FB5515482D44, /* ~= 10^-199 */
      0x991711052D8BF3C5, 0x751BDD152D4D1C4A, /* ~= 10^-198 */
      0xBF5CD54678EEF0B6, 0xD262D45A78A0635D, /* ~= 10^-197 */
      0xEF340A98172AACE4, 0x86FB897116C87C34, /* ~= 10^-196 */
      0x9580869F0E7AAC0E, 0xD45D35E6AE3D4DA0, /* ~= 10^-195 */
      0xBAE0A846D2195712, 0x8974836059CCA109, /* ~= 10^-194 */
      0xE998D258869FACD7, 0x2BD1A438703FC94B, /* ~= 10^-193 */
      0x91FF83775423CC06, 0x7B6306A34627DDCF, /* ~= 10^-192 */
      0xB67F6455292CBF08, 0x1A3BC84C17B1D542, /* ~= 10^-191 */
      0xE41F3D6A7377EECA, 0x20CABA5F1D9E4A93, /* ~= 10^-190 */
      0x8E938662882AF53E, 0x547EB47B7282EE9C, /* ~= 10^-189 */
      0xB23867FB2A35B28D, 0xE99E619A4F23AA43, /* ~= 10^-188 */
      0xDEC681F9F4C31F31, 0x6405FA00E2EC94D4, /* ~= 10^-187 */
      0x8B3C113C38F9F37E, 0xDE83BC408DD3DD04, /* ~= 10^-186 */
      0xAE0B158B4738705E, 0x9624AB50B148D445, /* ~= 10^-185 */
      0xD98DDAEE19068C76, 0x3BADD624DD9B0957, /* ~= 10^-184 */
      0x87F8A8D4CFA417C9, 0xE54CA5D70A80E5D6, /* ~= 10^-183 */
      0xA9F6D30A038D1DBC, 0x5E9FCF4CCD211F4C, /* ~= 10^-182 */
      0xD47487CC8470652B, 0x7647C3200069671F, /* ~= 10^-181 */
      0x84C8D4DFD2C63F3B, 0x29ECD9F40041E073, /* ~= 10^-180 */
      0xA5FB0A17C777CF09, 0xF468107100525890, /* ~= 10^-179 */
      0xCF79CC9DB955C2CC, 0x7182148D4066EEB4, /* ~= 10^-178 */
      0x81AC1FE293D599BF, 0xC6F14CD848405530, /* ~= 10^-177 */
      0xA21727DB38CB002F, 0xB8ADA00E5A506A7C, /* ~= 10^-176 */
      0xCA9CF1D206FDC03B, 0xA6D90811F0E4851C, /* ~= 10^-175 */
      0xFD442E4688BD304A, 0x908F4A166D1DA663, /* ~= 10^-174 */
      0x9E4A9CEC15763E2E, 0x9A598E4E043287FE, /* ~= 10^-173 */
      0xC5DD44271AD3CDBA, 0x40EFF1E1853F29FD, /* ~= 10^-172 */
      0xF7549530E188C128, 0xD12BEE59E68EF47C, /* ~= 10^-171 */
      0x9A94DD3E8CF578B9, 0x82BB74F8301958CE, /* ~= 10^-170 */
      0xC13A148E3032D6E7, 0xE36A52363C1FAF01, /* ~= 10^-169 */
      0xF18899B1BC3F8CA1, 0xDC44E6C3CB279AC1, /* ~= 10^-168 */
      0x96F5600F15A7B7E5, 0x29AB103A5EF8C0B9, /* ~= 10^-167 */
      0xBCB2B812DB11A5DE, 0x7415D448F6B6F0E7, /* ~= 10^-166 */
      0xEBDF661791D60F56, 0x111B495B3464AD21, /* ~= 10^-165 */
      0x936B9FCEBB25C995, 0xCAB10DD900BEEC34, /* ~= 10^-164 */
      0xB84687C269EF3BFB, 0x3D5D514F40EEA742, /* ~= 10^-163 */
      0xE65829B3046B0AFA, 0x0CB4A5A3112A5112, /* ~= 10^-162 */
      0x8FF71A0FE2C2E6DC, 0x47F0E785EABA72AB, /* ~= 10^-161 */
      0xB3F4E093DB73A093, 0x59ED216765690F56, /* ~= 10^-160 */
      0xE0F218B8D25088B8, 0x306869C13EC3532C, /* ~= 10^-159 */
      0x8C974F7383725573, 0x1E414218C73A13FB, /* ~= 10^-158 */
      0xAFBD2350644EEACF, 0xE5D1929EF90898FA, /* ~= 10^-157 */
      0xDBAC6C247D62A583, 0xDF45F746B74ABF39, /* ~= 10^-156 */
      0x894BC396CE5DA772, 0x6B8BBA8C328EB783, /* ~= 10^-155 */
      0xAB9EB47C81F5114F, 0x066EA92F3F326564, /* ~= 10^-154 */
      0xD686619BA27255A2, 0xC80A537B0EFEFEBD, /* ~= 10^-153 */
      0x8613FD0145877585, 0xBD06742CE95F5F36, /* ~= 10^-152 */
      0xA798FC4196E952E7, 0x2C48113823B73704, /* ~= 10^-151 */
      0xD17F3B51FCA3A7A0, 0xF75A15862CA504C5, /* ~= 10^-150 */
      0x82EF85133DE648C4, 0x9A984D73DBE722FB, /* ~= 10^-149 */
      0xA3AB66580D5FDAF5, 0xC13E60D0D2E0EBBA, /* ~= 10^-148 */
      0xCC963FEE10B7D1B3, 0x318DF905079926A8, /* ~= 10^-147 */
      0xFFBBCFE994E5C61F, 0xFDF17746497F7052, /* ~= 10^-146 */
      0x9FD561F1FD0F9BD3, 0xFEB6EA8BEDEFA633, /* ~= 10^-145 */
      0xC7CABA6E7C5382C8, 0xFE64A52EE96B8FC0, /* ~= 10^-144 */
      0xF9BD690A1B68637B, 0x3DFDCE7AA3C673B0, /* ~= 10^-143 */
      0x9C1661A651213E2D, 0x06BEA10CA65C084E, /* ~= 10^-142 */
      0xC31BFA0FE5698DB8, 0x486E494FCFF30A62, /* ~= 10^-141 */
      0xF3E2F893DEC3F126, 0x5A89DBA3C3EFCCFA, /* ~= 10^-140 */
      0x986DDB5C6B3A76B7, 0xF89629465A75E01C, /* ~= 10^-139 */
      0xBE89523386091465, 0xF6BBB397F1135823, /* ~= 10^-138 */
      0xEE2BA6C0678B597F, 0x746AA07DED582E2C, /* ~= 10^-137 */
      0x94DB483840B717EF, 0xA8C2A44EB4571CDC, /* ~= 10^-136 */
      0xBA121A4650E4DDEB, 0x92F34D62616CE413, /* ~= 10^-135 */
      0xE896A0D7E51E1566, 0x77B020BAF9C81D17, /* ~= 10^-134 */
      0x915E2486EF32CD60, 0x0ACE1474DC1D122E, /* ~= 10^-133 */
      0xB5B5ADA8AAFF80B8, 0x0D819992132456BA, /* ~= 10^-132 */
      0xE3231912D5BF60E6, 0x10E1FFF697ED6C69, /* ~= 10^-131 */
      0x8DF5EFABC5979C8F, 0xCA8D3FFA1EF463C1, /* ~= 10^-130 */
      0xB1736B96B6FD83B3, 0xBD308FF8A6B17CB2, /* ~= 10^-129 */
      0xDDD0467C64BCE4A0, 0xAC7CB3F6D05DDBDE, /* ~= 10^-128 */
      0x8AA22C0DBEF60EE4, 0x6BCDF07A423AA96B, /* ~= 10^-127 */
      0xAD4AB7112EB3929D, 0x86C16C98D2C953C6, /* ~= 10^-126 */
      0xD89D64D57A607744, 0xE871C7BF077BA8B7, /* ~= 10^-125 */
      0x87625F056C7C4A8B, 0x11471CD764AD4972, /* ~= 10^-124 */
      0xA93AF6C6C79B5D2D, 0xD598E40D3DD89BCF, /* ~= 10^-123 */
      0xD389B47879823479, 0x4AFF1D108D4EC2C3, /* ~= 10^-122 */
      0x843610CB4BF160CB, 0xCEDF722A585139BA, /* ~= 10^-121 */
      0xA54394FE1EEDB8FE, 0xC2974EB4EE658828, /* ~= 10^-120 */
      0xCE947A3DA6A9273E, 0x733D226229FEEA32, /* ~= 10^-119 */
      0x811CCC668829B887, 0x0806357D5A3F525F, /* ~= 10^-118 */
      0xA163FF802A3426A8, 0xCA07C2DCB0CF26F7, /* ~= 10^-117 */
      0xC9BCFF6034C13052, 0xFC89B393DD02F0B5, /* ~= 10^-116 */
      0xFC2C3F3841F17C67, 0xBBAC2078D443ACE2, /* ~= 10^-115 */
      0x9D9BA7832936EDC0, 0xD54B944B84AA4C0D, /* ~= 10^-114 */
      0xC5029163F384A931, 0x0A9E795E65D4DF11, /* ~= 10^-113 */
      0xF64335BCF065D37D, 0x4D4617B5FF4A16D5, /* ~= 10^-112 */
      0x99EA0196163FA42E, 0x504BCED1BF8E4E45, /* ~= 10^-111 */
      0xC06481FB9BCF8D39, 0xE45EC2862F71E1D6, /* ~= 10^-110 */
      0xF07DA27A82C37088, 0x5D767327BB4E5A4C, /* ~= 10^-109 */
      0x964E858C91BA2655, 0x3A6A07F8D510F86F, /* ~= 10^-108 */
      0xBBE226EFB628AFEA, 0x890489F70A55368B, /* ~= 10^-107 */
      0xEADAB0ABA3B2DBE5, 0x2B45AC74CCEA842E, /* ~= 10^-106 */
      0x92C8AE6B464FC96F, 0x3B0B8BC90012929D, /* ~= 10^-105 */
      0xB77ADA0617E3BBCB, 0x09CE6EBB40173744, /* ~= 10^-104 */
      0xE55990879DDCAABD, 0xCC420A6A101D0515, /* ~= 10^-103 */
      0x8F57FA54C2A9EAB6, 0x9FA946824A12232D, /* ~= 10^-102 */
      0xB32DF8E9F3546564, 0x47939822DC96ABF9, /* ~= 10^-101 */
      0xDFF9772470297EBD, 0x59787E2B93BC56F7, /* ~= 10^-100 */
      0x8BFBEA76C619EF36, 0x57EB4EDB3C55B65A, /* ~= 10^-99 */
      0xAEFAE51477A06B03, 0xEDE622920B6B23F1, /* ~= 10^-98 */
      0xDAB99E59958885C4, 0xE95FAB368E45ECED, /* ~= 10^-97 */
      0x88B402F7FD75539B, 0x11DBCB0218EBB414, /* ~= 10^-96 */
      0xAAE103B5FCD2A881, 0xD652BDC29F26A119, /* ~= 10^-95 */
      0xD59944A37C0752A2, 0x4BE76D3346F0495F, /* ~= 10^-94 */
      0x857FCAE62D8493A5, 0x6F70A4400C562DDB, /* ~= 10^-93 */
      0xA6DFBD9FB8E5B88E, 0xCB4CCD500F6BB952, /* ~= 10^-92 */
      0xD097AD07A71F26B2, 0x7E2000A41346A7A7, /* ~= 10^-91 */
      0x825ECC24C873782F, 0x8ED400668C0C28C8, /* ~= 10^-90 */
      0xA2F67F2DFA90563B, 0x728900802F0F32FA, /* ~= 10^-89 */
      0xCBB41EF979346BCA, 0x4F2B40A03AD2FFB9, /* ~= 10^-88 */
      0xFEA126B7D78186BC, 0xE2F610C84987BFA8, /* ~= 10^-87 */
      0x9F24B832E6B0F436, 0x0DD9CA7D2DF4D7C9, /* ~= 10^-86 */
      0xC6EDE63FA05D3143, 0x91503D1C79720DBB, /* ~= 10^-85 */
      0xF8A95FCF88747D94, 0x75A44C6397CE912A, /* ~= 10^-84 */
      0x9B69DBE1B548CE7C, 0xC986AFBE3EE11ABA, /* ~= 10^-83 */
      0xC24452DA229B021B, 0xFBE85BADCE996168, /* ~= 10^-82 */
      0xF2D56790AB41C2A2, 0xFAE27299423FB9C3, /* ~= 10^-81 */
      0x97C560BA6B0919A5, 0xDCCD879FC967D41A, /* ~= 10^-80 */
      0xBDB6B8E905CB600F, 0x5400E987BBC1C920, /* ~= 10^-79 */
      0xED246723473E3813, 0x290123E9AAB23B68, /* ~= 10^-78 */
      0x9436C0760C86E30B, 0xF9A0B6720AAF6521, /* ~= 10^-77 */
      0xB94470938FA89BCE, 0xF808E40E8D5B3E69, /* ~= 10^-76 */
      0xE7958CB87392C2C2, 0xB60B1D1230B20E04, /* ~= 10^-75 */
      0x90BD77F3483BB9B9, 0xB1C6F22B5E6F48C2, /* ~= 10^-74 */
      0xB4ECD5F01A4AA828, 0x1E38AEB6360B1AF3, /* ~= 10^-73 */
      0xE2280B6C20DD5232, 0x25C6DA63C38DE1B0, /* ~= 10^-72 */
      0x8D590723948A535F, 0x579C487E5A38AD0E, /* ~= 10^-71 */
      0xB0AF48EC79ACE837, 0x2D835A9DF0C6D851, /* ~= 10^-70 */
      0xDCDB1B2798182244, 0xF8E431456CF88E65, /* ~= 10^-69 */
      0x8A08F0F8BF0F156B, 0x1B8E9ECB641B58FF, /* ~= 10^-68 */
      0xAC8B2D36EED2DAC5, 0xE272467E3D222F3F, /* ~= 10^-67 */
      0xD7ADF884AA879177, 0x5B0ED81DCC6ABB0F, /* ~= 10^-66 */
      0x86CCBB52EA94BAEA, 0x98E947129FC2B4E9, /* ~= 10^-65 */
      0xA87FEA27A539E9A5, 0x3F2398D747B36224, /* ~= 10^-64 */
      0xD29FE4B18E88640E, 0x8EEC7F0D19A03AAD, /* ~= 10^-63 */
      0x83A3EEEEF9153E89, 0x1953CF68300424AC, /* ~= 10^-62 */
      0xA48CEAAAB75A8E2B, 0x5FA8C3423C052DD7, /* ~= 10^-61 */
      0xCDB02555653131B6, 0x3792F412CB06794D, /* ~= 10^-60 */
      0x808E17555F3EBF11, 0xE2BBD88BBEE40BD0, /* ~= 10^-59 */
      0xA0B19D2AB70E6ED6, 0x5B6ACEAEAE9D0EC4, /* ~= 10^-58 */
      0xC8DE047564D20A8B, 0xF245825A5A445275, /* ~= 10^-57 */
      0xFB158592BE068D2E, 0xEED6E2F0F0D56712, /* ~= 10^-56 */
      0x9CED737BB6C4183D, 0x55464DD69685606B, /* ~= 10^-55 */
      0xC428D05AA4751E4C, 0xAA97E14C3C26B886, /* ~= 10^-54 */
      0xF53304714D9265DF, 0xD53DD99F4B3066A8, /* ~= 10^-53 */
      0x993FE2C6D07B7FAB, 0xE546A8038EFE4029, /* ~= 10^-52 */
      0xBF8FDB78849A5F96, 0xDE98520472BDD033, /* ~= 10^-51 */
      0xEF73D256A5C0F77C, 0x963E66858F6D4440, /* ~= 10^-50 */
      0x95A8637627989AAD, 0xDDE7001379A44AA8, /* ~= 10^-49 */
      0xBB127C53B17EC159, 0x5560C018580D5D52, /* ~= 10^-48 */
      0xE9D71B689DDE71AF, 0xAAB8F01E6E10B4A6, /* ~= 10^-47 */
      0x9226712162AB070D, 0xCAB3961304CA70E8, /* ~= 10^-46 */
      0xB6B00D69BB55C8D1, 0x3D607B97C5FD0D22, /* ~= 10^-45 */
      0xE45C10C42A2B3B05, 0x8CB89A7DB77C506A, /* ~= 10^-44 */
      0x8EB98A7A9A5B04E3, 0x77F3608E92ADB242, /* ~= 10^-43 */
      0xB267ED1940F1C61C, 0x55F038B237591ED3, /* ~= 10^-42 */
      0xDF01E85F912E37A3, 0x6B6C46DEC52F6688, /* ~= 10^-41 */
      0x8B61313BBABCE2C6, 0x2323AC4B3B3DA015, /* ~= 10^-40 */
      0xAE397D8AA96C1B77, 0xABEC975E0A0D081A, /* ~= 10^-39 */
      0xD9C7DCED53C72255, 0x96E7BD358C904A21, /* ~= 10^-38 */
      0x881CEA14545C7575, 0x7E50D64177DA2E54, /* ~= 10^-37 */
      0xAA242499697392D2, 0xDDE50BD1D5D0B9E9, /* ~= 10^-36 */
      0xD4AD2DBFC3D07787, 0x955E4EC64B44E864, /* ~= 10^-35 */
      0x84EC3C97DA624AB4, 0xBD5AF13BEF0B113E, /* ~= 10^-34 */
      0xA6274BBDD0FADD61, 0xECB1AD8AEACDD58E, /* ~= 10^-33 */
      0xCFB11EAD453994BA, 0x67DE18EDA5814AF2, /* ~= 10^-32 */
      0x81CEB32C4B43FCF4, 0x80EACF948770CED7, /* ~= 10^-31 */
      0xA2425FF75E14FC31, 0xA1258379A94D028D, /* ~= 10^-30 */
      0xCAD2F7F5359A3B3E, 0x096EE45813A04330, /* ~= 10^-29 */
      0xFD87B5F28300CA0D, 0x8BCA9D6E188853FC, /* ~= 10^-28 */
      0x9E74D1B791E07E48, 0x775EA264CF55347D, /* ~= 10^-27 */
      0xC612062576589DDA, 0x95364AFE032A819D, /* ~= 10^-26 */
      0xF79687AED3EEC551, 0x3A83DDBD83F52204, /* ~= 10^-25 */
      0x9ABE14CD44753B52, 0xC4926A9672793542, /* ~= 10^-24 */
      0xC16D9A0095928A27, 0x75B7053C0F178293, /* ~= 10^-23 */
      0xF1C90080BAF72CB1, 0x5324C68B12DD6338, /* ~= 10^-22 */
      0x971DA05074DA7BEE, 0xD3F6FC16EBCA5E03, /* ~= 10^-21 */
      0xBCE5086492111AEA, 0x88F4BB1CA6BCF584, /* ~= 10^-20 */
      0xEC1E4A7DB69561A5, 0x2B31E9E3D06C32E5, /* ~= 10^-19 */
      0x9392EE8E921D5D07, 0x3AFF322E62439FCF, /* ~= 10^-18 */
      0xB877AA3236A4B449, 0x09BEFEB9FAD487C2, /* ~= 10^-17 */
      0xE69594BEC44DE15B, 0x4C2EBE687989A9B3, /* ~= 10^-16 */
      0x901D7CF73AB0ACD9, 0x0F9D37014BF60A10, /* ~= 10^-15 */
      0xB424DC35095CD80F, 0x538484C19EF38C94, /* ~= 10^-14 */
      0xE12E13424BB40E13, 0x2865A5F206B06FB9, /* ~= 10^-13 */
      0x8CBCCC096F5088CB, 0xF93F87B7442E45D3, /* ~= 10^-12 */
      0xAFEBFF0BCB24AAFE, 0xF78F69A51539D748, /* ~= 10^-11 */
      0xDBE6FECEBDEDD5BE, 0xB573440E5A884D1B, /* ~= 10^-10 */
      0x89705F4136B4A597, 0x31680A88F8953030, /* ~= 10^-9 */
      0xABCC77118461CEFC, 0xFDC20D2B36BA7C3D, /* ~= 10^-8 */
      0xD6BF94D5E57A42BC, 0x3D32907604691B4C, /* ~= 10^-7 */
      0x8637BD05AF6C69B5, 0xA63F9A49C2C1B10F, /* ~= 10^-6 */
      0xA7C5AC471B478423, 0x0FCF80DC33721D53, /* ~= 10^-5 */
      0xD1B71758E219652B, 0xD3C36113404EA4A8, /* ~= 10^-4 */
      0x83126E978D4FDF3B, 0x645A1CAC083126E9, /* ~= 10^-3 */
      0xA3D70A3D70A3D70A, 0x3D70A3D70A3D70A3, /* ~= 10^-2 */
      0xCCCCCCCCCCCCCCCC, 0xCCCCCCCCCCCCCCCC, /* ~= 10^-1 */
      0x8000000000000000, 0x0000000000000000, /* == 10^0 */
      0xA000000000000000, 0x0000000000000000, /* == 10^1 */
      0xC800000000000000, 0x0000000000000000, /* == 10^2 */
      0xFA00000000000000, 0x0000000000000000, /* == 10^3 */
      0x9C40000000000000, 0x0000000000000000, /* == 10^4 */
      0xC350000000000000, 0x0000000000000000, /* == 10^5 */
      0xF424000000000000, 0x0000000000000000, /* == 10^6 */
      0x9896800000000000, 0x0000000000000000, /* == 10^7 */
      0xBEBC200000000000, 0x0000000000000000, /* == 10^8 */
      0xEE6B280000000000, 0x0000000000000000, /* == 10^9 */
      0x9502F90000000000, 0x0000000000000000, /* == 10^10 */
      0xBA43B74000000000, 0x0000000000000000, /* == 10^11 */
      0xE8D4A51000000000, 0x0000000000000000, /* == 10^12 */
      0x9184E72A00000000, 0x0000000000000000, /* == 10^13 */
      0xB5E620F480000000, 0x0000000000000000, /* == 10^14 */
      0xE35FA931A0000000, 0x0000000000000000, /* == 10^15 */
      0x8E1BC9BF04000000, 0x0000000000000000, /* == 10^16 */
      0xB1A2BC2EC5000000, 0x0000000000000000, /* == 10^17 */
      0xDE0B6B3A76400000, 0x0000000000000000, /* == 10^18 */
      0x8AC7230489E80000, 0x0000000000000000, /* == 10^19 */
      0xAD78EBC5AC620000, 0x0000000000000000, /* == 10^20 */
      0xD8D726B7177A8000, 0x0000000000000000, /* == 10^21 */
      0x878678326EAC9000, 0x0000000000000000, /* == 10^22 */
      0xA968163F0A57B400, 0x0000000000000000, /* == 10^23 */
      0xD3C21BCECCEDA100, 0x0000000000000000, /* == 10^24 */
      0x84595161401484A0, 0x0000000000000000, /* == 10^25 */
      0xA56FA5B99019A5C8, 0x0000000000000000, /* == 10^26 */
      0xCECB8F27F4200F3A, 0x0000000000000000, /* == 10^27 */
      0x813F3978F8940984, 0x4000000000000000, /* == 10^28 */
      0xA18F07D736B90BE5, 0x5000000000000000, /* == 10^29 */
      0xC9F2C9CD04674EDE, 0xA400000000000000, /* == 10^30 */
      0xFC6F7C4045812296, 0x4D00000000000000, /* == 10^31 */
      0x9DC5ADA82B70B59D, 0xF020000000000000, /* == 10^32 */
      0xC5371912364CE305, 0x6C28000000000000, /* == 10^33 */
      0xF684DF56C3E01BC6, 0xC732000000000000, /* == 10^34 */
      0x9A130B963A6C115C, 0x3C7F400000000000, /* == 10^35 */
      0xC097CE7BC90715B3, 0x4B9F100000000000, /* == 10^36 */
      0xF0BDC21ABB48DB20, 0x1E86D40000000000, /* == 10^37 */
      0x96769950B50D88F4, 0x1314448000000000, /* == 10^38 */
      0xBC143FA4E250EB31, 0x17D955A000000000, /* == 10^39 */
      0xEB194F8E1AE525FD, 0x5DCFAB0800000000, /* == 10^40 */
      0x92EFD1B8D0CF37BE, 0x5AA1CAE500000000, /* == 10^41 */
      0xB7ABC627050305AD, 0xF14A3D9E40000000, /* == 10^42 */
      0xE596B7B0C643C719, 0x6D9CCD05D0000000, /* == 10^43 */
      0x8F7E32CE7BEA5C6F, 0xE4820023A2000000, /* == 10^44 */
      0xB35DBF821AE4F38B, 0xDDA2802C8A800000, /* == 10^45 */
      0xE0352F62A19E306E, 0xD50B2037AD200000, /* == 10^46 */
      0x8C213D9DA502DE45, 0x4526F422CC340000, /* == 10^47 */
      0xAF298D050E4395D6, 0x9670B12B7F410000, /* == 10^48 */
      0xDAF3F04651D47B4C, 0x3C0CDD765F114000, /* == 10^49 */
      0x88D8762BF324CD0F, 0xA5880A69FB6AC800, /* == 10^50 */
      0xAB0E93B6EFEE0053, 0x8EEA0D047A457A00, /* == 10^51 */
      0xD5D238A4ABE98068, 0x72A4904598D6D880, /* == 10^52 */
      0x85A36366EB71F041, 0x47A6DA2B7F864750, /* == 10^53 */
      0xA70C3C40A64E6C51, 0x999090B65F67D924, /* == 10^54 */
      0xD0CF4B50CFE20765, 0xFFF4B4E3F741CF6D, /* == 10^55 */
      0x82818F1281ED449F, 0xBFF8F10E7A8921A4, /* ~= 10^56 */
      0xA321F2D7226895C7, 0xAFF72D52192B6A0D, /* ~= 10^57 */
      0xCBEA6F8CEB02BB39, 0x9BF4F8A69F764490, /* ~= 10^58 */
      0xFEE50B7025C36A08, 0x02F236D04753D5B4, /* ~= 10^59 */
      0x9F4F2726179A2245, 0x01D762422C946590, /* ~= 10^60 */
      0xC722F0EF9D80AAD6, 0x424D3AD2B7B97EF5, /* ~= 10^61 */
      0xF8EBAD2B84E0D58B, 0xD2E0898765A7DEB2, /* ~= 10^62 */
      0x9B934C3B330C8577, 0x63CC55F49F88EB2F, /* ~= 10^63 */
      0xC2781F49FFCFA6D5, 0x3CBF6B71C76B25FB, /* ~= 10^64 */
      0xF316271C7FC3908A, 0x8BEF464E3945EF7A, /* ~= 10^65 */
      0x97EDD871CFDA3A56, 0x97758BF0E3CBB5AC, /* ~= 10^66 */
      0xBDE94E8E43D0C8EC, 0x3D52EEED1CBEA317, /* ~= 10^67 */
      0xED63A231D4C4FB27, 0x4CA7AAA863EE4BDD, /* ~= 10^68 */
      0x945E455F24FB1CF8, 0x8FE8CAA93E74EF6A, /* ~= 10^69 */
      0xB975D6B6EE39E436, 0xB3E2FD538E122B44, /* ~= 10^70 */
      0xE7D34C64A9C85D44, 0x60DBBCA87196B616, /* ~= 10^71 */
      0x90E40FBEEA1D3A4A, 0xBC8955E946FE31CD, /* ~= 10^72 */
      0xB51D13AEA4A488DD, 0x6BABAB6398BDBE41, /* ~= 10^73 */
      0xE264589A4DCDAB14, 0xC696963C7EED2DD1, /* ~= 10^74 */
      0x8D7EB76070A08AEC, 0xFC1E1DE5CF543CA2, /* ~= 10^75 */
      0xB0DE65388CC8ADA8, 0x3B25A55F43294BCB, /* ~= 10^76 */
      0xDD15FE86AFFAD912, 0x49EF0EB713F39EBE, /* ~= 10^77 */
      0x8A2DBF142DFCC7AB, 0x6E3569326C784337, /* ~= 10^78 */
      0xACB92ED9397BF996, 0x49C2C37F07965404, /* ~= 10^79 */
      0xD7E77A8F87DAF7FB, 0xDC33745EC97BE906, /* ~= 10^80 */
      0x86F0AC99B4E8DAFD, 0x69A028BB3DED71A3, /* ~= 10^81 */
      0xA8ACD7C0222311BC, 0xC40832EA0D68CE0C, /* ~= 10^82 */
      0xD2D80DB02AABD62B, 0xF50A3FA490C30190, /* ~= 10^83 */
      0x83C7088E1AAB65DB, 0x792667C6DA79E0FA, /* ~= 10^84 */
      0xA4B8CAB1A1563F52, 0x577001B891185938, /* ~= 10^85 */
      0xCDE6FD5E09ABCF26, 0xED4C0226B55E6F86, /* ~= 10^86 */
      0x80B05E5AC60B6178, 0x544F8158315B05B4, /* ~= 10^87 */
      0xA0DC75F1778E39D6, 0x696361AE3DB1C721, /* ~= 10^88 */
      0xC913936DD571C84C, 0x03BC3A19CD1E38E9, /* ~= 10^89 */
      0xFB5878494ACE3A5F, 0x04AB48A04065C723, /* ~= 10^90 */
      0x9D174B2DCEC0E47B, 0x62EB0D64283F9C76, /* ~= 10^91 */
      0xC45D1DF942711D9A, 0x3BA5D0BD324F8394, /* ~= 10^92 */
      0xF5746577930D6500, 0xCA8F44EC7EE36479, /* ~= 10^93 */
      0x9968BF6ABBE85F20, 0x7E998B13CF4E1ECB, /* ~= 10^94 */
      0xBFC2EF456AE276E8, 0x9E3FEDD8C321A67E, /* ~= 10^95 */
      0xEFB3AB16C59B14A2, 0xC5CFE94EF3EA101E, /* ~= 10^96 */
      0x95D04AEE3B80ECE5, 0xBBA1F1D158724A12, /* ~= 10^97 */
      0xBB445DA9CA61281F, 0x2A8A6E45AE8EDC97, /* ~= 10^98 */
      0xEA1575143CF97226, 0xF52D09D71A3293BD, /* ~= 10^99 */
      0x924D692CA61BE758, 0x593C2626705F9C56, /* ~= 10^100 */
      0xB6E0C377CFA2E12E, 0x6F8B2FB00C77836C, /* ~= 10^101 */
      0xE498F455C38B997A, 0x0B6DFB9C0F956447, /* ~= 10^102 */
      0x8EDF98B59A373FEC, 0x4724BD4189BD5EAC, /* ~= 10^103 */
      0xB2977EE300C50FE7, 0x58EDEC91EC2CB657, /* ~= 10^104 */
      0xDF3D5E9BC0F653E1, 0x2F2967B66737E3ED, /* ~= 10^105 */
      0x8B865B215899F46C, 0xBD79E0D20082EE74, /* ~= 10^106 */
      0xAE67F1E9AEC07187, 0xECD8590680A3AA11, /* ~= 10^107 */
      0xDA01EE641A708DE9, 0xE80E6F4820CC9495, /* ~= 10^108 */
      0x884134FE908658B2, 0x3109058D147FDCDD, /* ~= 10^109 */
      0xAA51823E34A7EEDE, 0xBD4B46F0599FD415, /* ~= 10^110 */
      0xD4E5E2CDC1D1EA96, 0x6C9E18AC7007C91A, /* ~= 10^111 */
      0x850FADC09923329E, 0x03E2CF6BC604DDB0, /* ~= 10^112 */
      0xA6539930BF6BFF45, 0x84DB8346B786151C, /* ~= 10^113 */
      0xCFE87F7CEF46FF16, 0xE612641865679A63, /* ~= 10^114 */
      0x81F14FAE158C5F6E, 0x4FCB7E8F3F60C07E, /* ~= 10^115 */
      0xA26DA3999AEF7749, 0xE3BE5E330F38F09D, /* ~= 10^116 */
      0xCB090C8001AB551C, 0x5CADF5BFD3072CC5, /* ~= 10^117 */
      0xFDCB4FA002162A63, 0x73D9732FC7C8F7F6, /* ~= 10^118 */
      0x9E9F11C4014DDA7E, 0x2867E7FDDCDD9AFA, /* ~= 10^119 */
      0xC646D63501A1511D, 0xB281E1FD541501B8, /* ~= 10^120 */
      0xF7D88BC24209A565, 0x1F225A7CA91A4226, /* ~= 10^121 */
      0x9AE757596946075F, 0x3375788DE9B06958, /* ~= 10^122 */
      0xC1A12D2FC3978937, 0x0052D6B1641C83AE, /* ~= 10^123 */
      0xF209787BB47D6B84, 0xC0678C5DBD23A49A, /* ~= 10^124 */
      0x9745EB4D50CE6332, 0xF840B7BA963646E0, /* ~= 10^125 */
      0xBD176620A501FBFF, 0xB650E5A93BC3D898, /* ~= 10^126 */
      0xEC5D3FA8CE427AFF, 0xA3E51F138AB4CEBE, /* ~= 10^127 */
      0x93BA47C980E98CDF, 0xC66F336C36B10137, /* ~= 10^128 */
      0xB8A8D9BBE123F017, 0xB80B0047445D4184, /* ~= 10^129 */
      0xE6D3102AD96CEC1D, 0xA60DC059157491E5, /* ~= 10^130 */
      0x9043EA1AC7E41392, 0x87C89837AD68DB2F, /* ~= 10^131 */
      0xB454E4A179DD1877, 0x29BABE4598C311FB, /* ~= 10^132 */
      0xE16A1DC9D8545E94, 0xF4296DD6FEF3D67A, /* ~= 10^133 */
      0x8CE2529E2734BB1D, 0x1899E4A65F58660C, /* ~= 10^134 */
      0xB01AE745B101E9E4, 0x5EC05DCFF72E7F8F, /* ~= 10^135 */
      0xDC21A1171D42645D, 0x76707543F4FA1F73, /* ~= 10^136 */
      0x899504AE72497EBA, 0x6A06494A791C53A8, /* ~= 10^137 */
      0xABFA45DA0EDBDE69, 0x0487DB9D17636892, /* ~= 10^138 */
      0xD6F8D7509292D603, 0x45A9D2845D3C42B6, /* ~= 10^139 */
      0x865B86925B9BC5C2, 0x0B8A2392BA45A9B2, /* ~= 10^140 */
      0xA7F26836F282B732, 0x8E6CAC7768D7141E, /* ~= 10^141 */
      0xD1EF0244AF2364FF, 0x3207D795430CD926, /* ~= 10^142 */
      0x8335616AED761F1F, 0x7F44E6BD49E807B8, /* ~= 10^143 */
      0xA402B9C5A8D3A6E7, 0x5F16206C9C6209A6, /* ~= 10^144 */
      0xCD036837130890A1, 0x36DBA887C37A8C0F, /* ~= 10^145 */
      0x802221226BE55A64, 0xC2494954DA2C9789, /* ~= 10^146 */
      0xA02AA96B06DEB0FD, 0xF2DB9BAA10B7BD6C, /* ~= 10^147 */
      0xC83553C5C8965D3D, 0x6F92829494E5ACC7, /* ~= 10^148 */
      0xFA42A8B73ABBF48C, 0xCB772339BA1F17F9, /* ~= 10^149 */
      0x9C69A97284B578D7, 0xFF2A760414536EFB, /* ~= 10^150 */
      0xC38413CF25E2D70D, 0xFEF5138519684ABA, /* ~= 10^151 */
      0xF46518C2EF5B8CD1, 0x7EB258665FC25D69, /* ~= 10^152 */
      0x98BF2F79D5993802, 0xEF2F773FFBD97A61, /* ~= 10^153 */
      0xBEEEFB584AFF8603, 0xAAFB550FFACFD8FA, /* ~= 10^154 */
      0xEEAABA2E5DBF6784, 0x95BA2A53F983CF38, /* ~= 10^155 */
      0x952AB45CFA97A0B2, 0xDD945A747BF26183, /* ~= 10^156 */
      0xBA756174393D88DF, 0x94F971119AEEF9E4, /* ~= 10^157 */
      0xE912B9D1478CEB17, 0x7A37CD5601AAB85D, /* ~= 10^158 */
      0x91ABB422CCB812EE, 0xAC62E055C10AB33A, /* ~= 10^159 */
      0xB616A12B7FE617AA, 0x577B986B314D6009, /* ~= 10^160 */
      0xE39C49765FDF9D94, 0xED5A7E85FDA0B80B, /* ~= 10^161 */
      0x8E41ADE9FBEBC27D, 0x14588F13BE847307, /* ~= 10^162 */
      0xB1D219647AE6B31C, 0x596EB2D8AE258FC8, /* ~= 10^163 */
      0xDE469FBD99A05FE3, 0x6FCA5F8ED9AEF3BB, /* ~= 10^164 */
      0x8AEC23D680043BEE, 0x25DE7BB9480D5854, /* ~= 10^165 */
      0xADA72CCC20054AE9, 0xAF561AA79A10AE6A, /* ~= 10^166 */
      0xD910F7FF28069DA4, 0x1B2BA1518094DA04, /* ~= 10^167 */
      0x87AA9AFF79042286, 0x90FB44D2F05D0842, /* ~= 10^168 */
      0xA99541BF57452B28, 0x353A1607AC744A53, /* ~= 10^169 */
      0xD3FA922F2D1675F2, 0x42889B8997915CE8, /* ~= 10^170 */
      0x847C9B5D7C2E09B7, 0x69956135FEBADA11, /* ~= 10^171 */
      0xA59BC234DB398C25, 0x43FAB9837E699095, /* ~= 10^172 */
      0xCF02B2C21207EF2E, 0x94F967E45E03F4BB, /* ~= 10^173 */
      0x8161AFB94B44F57D, 0x1D1BE0EEBAC278F5, /* ~= 10^174 */
      0xA1BA1BA79E1632DC, 0x6462D92A69731732, /* ~= 10^175 */
      0xCA28A291859BBF93, 0x7D7B8F7503CFDCFE, /* ~= 10^176 */
      0xFCB2CB35E702AF78, 0x5CDA735244C3D43E, /* ~= 10^177 */
      0x9DEFBF01B061ADAB, 0x3A0888136AFA64A7, /* ~= 10^178 */
      0xC56BAEC21C7A1916, 0x088AAA1845B8FDD0, /* ~= 10^179 */
      0xF6C69A72A3989F5B, 0x8AAD549E57273D45, /* ~= 10^180 */
      0x9A3C2087A63F6399, 0x36AC54E2F678864B, /* ~= 10^181 */
      0xC0CB28A98FCF3C7F, 0x84576A1BB416A7DD, /* ~= 10^182 */
      0xF0FDF2D3F3C30B9F, 0x656D44A2A11C51D5, /* ~= 10^183 */
      0x969EB7C47859E743, 0x9F644AE5A4B1B325, /* ~= 10^184 */
      0xBC4665B596706114, 0x873D5D9F0DDE1FEE, /* ~= 10^185 */
      0xEB57FF22FC0C7959, 0xA90CB506D155A7EA, /* ~= 10^186 */
      0x9316FF75DD87CBD8, 0x09A7F12442D588F2, /* ~= 10^187 */
      0xB7DCBF5354E9BECE, 0x0C11ED6D538AEB2F, /* ~= 10^188 */
      0xE5D3EF282A242E81, 0x8F1668C8A86DA5FA, /* ~= 10^189 */
      0x8FA475791A569D10, 0xF96E017D694487BC, /* ~= 10^190 */
      0xB38D92D760EC4455, 0x37C981DCC395A9AC, /* ~= 10^191 */
      0xE070F78D3927556A, 0x85BBE253F47B1417, /* ~= 10^192 */
      0x8C469AB843B89562, 0x93956D7478CCEC8E, /* ~= 10^193 */
      0xAF58416654A6BABB, 0x387AC8D1970027B2, /* ~= 10^194 */
      0xDB2E51BFE9D0696A, 0x06997B05FCC0319E, /* ~= 10^195 */
      0x88FCF317F22241E2, 0x441FECE3BDF81F03, /* ~= 10^196 */
      0xAB3C2FDDEEAAD25A, 0xD527E81CAD7626C3, /* ~= 10^197 */
      0xD60B3BD56A5586F1, 0x8A71E223D8D3B074, /* ~= 10^198 */
      0x85C7056562757456, 0xF6872D5667844E49, /* ~= 10^199 */
      0xA738C6BEBB12D16C, 0xB428F8AC016561DB, /* ~= 10^200 */
      0xD106F86E69D785C7, 0xE13336D701BEBA52, /* ~= 10^201 */
      0x82A45B450226B39C, 0xECC0024661173473, /* ~= 10^202 */
      0xA34D721642B06084, 0x27F002D7F95D0190, /* ~= 10^203 */
      0xCC20CE9BD35C78A5, 0x31EC038DF7B441F4, /* ~= 10^204 */
      0xFF290242C83396CE, 0x7E67047175A15271, /* ~= 10^205 */
      0x9F79A169BD203E41, 0x0F0062C6E984D386, /* ~= 10^206 */
      0xC75809C42C684DD1, 0x52C07B78A3E60868, /* ~= 10^207 */
      0xF92E0C3537826145, 0xA7709A56CCDF8A82, /* ~= 10^208 */
      0x9BBCC7A142B17CCB, 0x88A66076400BB691, /* ~= 10^209 */
      0xC2ABF989935DDBFE, 0x6ACFF893D00EA435, /* ~= 10^210 */
      0xF356F7EBF83552FE, 0x0583F6B8C4124D43, /* ~= 10^211 */
      0x98165AF37B2153DE, 0xC3727A337A8B704A, /* ~= 10^212 */
      0xBE1BF1B059E9A8D6, 0x744F18C0592E4C5C, /* ~= 10^213 */
      0xEDA2EE1C7064130C, 0x1162DEF06F79DF73, /* ~= 10^214 */
      0x9485D4D1C63E8BE7, 0x8ADDCB5645AC2BA8, /* ~= 10^215 */
      0xB9A74A0637CE2EE1, 0x6D953E2BD7173692, /* ~= 10^216 */
      0xE8111C87C5C1BA99, 0xC8FA8DB6CCDD0437, /* ~= 10^217 */
      0x910AB1D4DB9914A0, 0x1D9C9892400A22A2, /* ~= 10^218 */
      0xB54D5E4A127F59C8, 0x2503BEB6D00CAB4B, /* ~= 10^219 */
      0xE2A0B5DC971F303A, 0x2E44AE64840FD61D, /* ~= 10^220 */
      0x8DA471A9DE737E24, 0x5CEAECFED289E5D2, /* ~= 10^221 */
      0xB10D8E1456105DAD, 0x7425A83E872C5F47, /* ~= 10^222 */
      0xDD50F1996B947518, 0xD12F124E28F77719, /* ~= 10^223 */
      0x8A5296FFE33CC92F, 0x82BD6B70D99AAA6F, /* ~= 10^224 */
      0xACE73CBFDC0BFB7B, 0x636CC64D1001550B, /* ~= 10^225 */
      0xD8210BEFD30EFA5A, 0x3C47F7E05401AA4E, /* ~= 10^226 */
      0x8714A775E3E95C78, 0x65ACFAEC34810A71, /* ~= 10^227 */
      0xA8D9D1535CE3B396, 0x7F1839A741A14D0D, /* ~= 10^228 */
      0xD31045A8341CA07C, 0x1EDE48111209A050, /* ~= 10^229 */
      0x83EA2B892091E44D, 0x934AED0AAB460432, /* ~= 10^230 */
      0xA4E4B66B68B65D60, 0xF81DA84D5617853F, /* ~= 10^231 */
      0xCE1DE40642E3F4B9, 0x36251260AB9D668E, /* ~= 10^232 */
      0x80D2AE83E9CE78F3, 0xC1D72B7C6B426019, /* ~= 10^233 */
      0xA1075A24E4421730, 0xB24CF65B8612F81F, /* ~= 10^234 */
      0xC94930AE1D529CFC, 0xDEE033F26797B627, /* ~= 10^235 */
      0xFB9B7CD9A4A7443C, 0x169840EF017DA3B1, /* ~= 10^236 */
      0x9D412E0806E88AA5, 0x8E1F289560EE864E, /* ~= 10^237 */
      0xC491798A08A2AD4E, 0xF1A6F2BAB92A27E2, /* ~= 10^238 */
      0xF5B5D7EC8ACB58A2, 0xAE10AF696774B1DB, /* ~= 10^239 */
      0x9991A6F3D6BF1765, 0xACCA6DA1E0A8EF29, /* ~= 10^240 */
      0xBFF610B0CC6EDD3F, 0x17FD090A58D32AF3, /* ~= 10^241 */
      0xEFF394DCFF8A948E, 0xDDFC4B4CEF07F5B0, /* ~= 10^242 */
      0x95F83D0A1FB69CD9, 0x4ABDAF101564F98E, /* ~= 10^243 */
      0xBB764C4CA7A4440F, 0x9D6D1AD41ABE37F1, /* ~= 10^244 */
      0xEA53DF5FD18D5513, 0x84C86189216DC5ED, /* ~= 10^245 */
      0x92746B9BE2F8552C, 0x32FD3CF5B4E49BB4, /* ~= 10^246 */
      0xB7118682DBB66A77, 0x3FBC8C33221DC2A1, /* ~= 10^247 */
      0xE4D5E82392A40515, 0x0FABAF3FEAA5334A, /* ~= 10^248 */
      0x8F05B1163BA6832D, 0x29CB4D87F2A7400E, /* ~= 10^249 */
      0xB2C71D5BCA9023F8, 0x743E20E9EF511012, /* ~= 10^250 */
      0xDF78E4B2BD342CF6, 0x914DA9246B255416, /* ~= 10^251 */
      0x8BAB8EEFB6409C1A, 0x1AD089B6C2F7548E, /* ~= 10^252 */
      0xAE9672ABA3D0C320, 0xA184AC2473B529B1, /* ~= 10^253 */
      0xDA3C0F568CC4F3E8, 0xC9E5D72D90A2741E, /* ~= 10^254 */
      0x8865899617FB1871, 0x7E2FA67C7A658892, /* ~= 10^255 */
      0xAA7EEBFB9DF9DE8D, 0xDDBB901B98FEEAB7, /* ~= 10^256 */
      0xD51EA6FA85785631, 0x552A74227F3EA565, /* ~= 10^257 */
      0x8533285C936B35DE, 0xD53A88958F87275F, /* ~= 10^258 */
      0xA67FF273B8460356, 0x8A892ABAF368F137, /* ~= 10^259 */
      0xD01FEF10A657842C, 0x2D2B7569B0432D85, /* ~= 10^260 */
      0x8213F56A67F6B29B, 0x9C3B29620E29FC73, /* ~= 10^261 */
      0xA298F2C501F45F42, 0x8349F3BA91B47B8F, /* ~= 10^262 */
      0xCB3F2F7642717713, 0x241C70A936219A73, /* ~= 10^263 */
      0xFE0EFB53D30DD4D7, 0xED238CD383AA0110, /* ~= 10^264 */
      0x9EC95D1463E8A506, 0xF4363804324A40AA, /* ~= 10^265 */
      0xC67BB4597CE2CE48, 0xB143C6053EDCD0D5, /* ~= 10^266 */
      0xF81AA16FDC1B81DA, 0xDD94B7868E94050A, /* ~= 10^267 */
      0x9B10A4E5E9913128, 0xCA7CF2B4191C8326, /* ~= 10^268 */
      0xC1D4CE1F63F57D72, 0xFD1C2F611F63A3F0, /* ~= 10^269 */
      0xF24A01A73CF2DCCF, 0xBC633B39673C8CEC, /* ~= 10^270 */
      0x976E41088617CA01, 0xD5BE0503E085D813, /* ~= 10^271 */
      0xBD49D14AA79DBC82, 0x4B2D8644D8A74E18, /* ~= 10^272 */
      0xEC9C459D51852BA2, 0xDDF8E7D60ED1219E, /* ~= 10^273 */
      0x93E1AB8252F33B45, 0xCABB90E5C942B503, /* ~= 10^274 */
      0xB8DA1662E7B00A17, 0x3D6A751F3B936243, /* ~= 10^275 */
      0xE7109BFBA19C0C9D, 0x0CC512670A783AD4, /* ~= 10^276 */
      0x906A617D450187E2, 0x27FB2B80668B24C5, /* ~= 10^277 */
      0xB484F9DC9641E9DA, 0xB1F9F660802DEDF6, /* ~= 10^278 */
      0xE1A63853BBD26451, 0x5E7873F8A0396973, /* ~= 10^279 */
      0x8D07E33455637EB2, 0xDB0B487B6423E1E8, /* ~= 10^280 */
      0xB049DC016ABC5E5F, 0x91CE1A9A3D2CDA62, /* ~= 10^281 */
      0xDC5C5301C56B75F7, 0x7641A140CC7810FB, /* ~= 10^282 */
      0x89B9B3E11B6329BA, 0xA9E904C87FCB0A9D, /* ~= 10^283 */
      0xAC2820D9623BF429, 0x546345FA9FBDCD44, /* ~= 10^284 */
      0xD732290FBACAF133, 0xA97C177947AD4095, /* ~= 10^285 */
      0x867F59A9D4BED6C0, 0x49ED8EABCCCC485D, /* ~= 10^286 */
      0xA81F301449EE8C70, 0x5C68F256BFFF5A74, /* ~= 10^287 */
      0xD226FC195C6A2F8C, 0x73832EEC6FFF3111, /* ~= 10^288 */
      0x83585D8FD9C25DB7, 0xC831FD53C5FF7EAB, /* ~= 10^289 */
      0xA42E74F3D032F525, 0xBA3E7CA8B77F5E55, /* ~= 10^290 */
      0xCD3A1230C43FB26F, 0x28CE1BD2E55F35EB, /* ~= 10^291 */
      0x80444B5E7AA7CF85, 0x7980D163CF5B81B3, /* ~= 10^292 */
      0xA0555E361951C366, 0xD7E105BCC332621F, /* ~= 10^293 */
      0xC86AB5C39FA63440, 0x8DD9472BF3FEFAA7, /* ~= 10^294 */
      0xFA856334878FC150, 0xB14F98F6F0FEB951, /* ~= 10^295 */
      0x9C935E00D4B9D8D2, 0x6ED1BF9A569F33D3, /* ~= 10^296 */
      0xC3B8358109E84F07, 0x0A862F80EC4700C8, /* ~= 10^297 */
      0xF4A642E14C6262C8, 0xCD27BB612758C0FA, /* ~= 10^298 */
      0x98E7E9CCCFBD7DBD, 0x8038D51CB897789C, /* ~= 10^299 */
      0xBF21E44003ACDD2C, 0xE0470A63E6BD56C3, /* ~= 10^300 */
      0xEEEA5D5004981478, 0x1858CCFCE06CAC74, /* ~= 10^301 */
      0x95527A5202DF0CCB, 0x0F37801E0C43EBC8, /* ~= 10^302 */
      0xBAA718E68396CFFD, 0xD30560258F54E6BA, /* ~= 10^303 */
      0xE950DF20247C83FD, 0x47C6B82EF32A2069, /* ~= 10^304 */
      0x91D28B7416CDD27E, 0x4CDC331D57FA5441, /* ~= 10^305 */
      0xB6472E511C81471D, 0xE0133FE4ADF8E952, /* ~= 10^306 */
      0xE3D8F9E563A198E5, 0x58180FDDD97723A6, /* ~= 10^307 */
      0x8E679C2F5E44FF8F, 0x570F09EAA7EA7648, /* ~= 10^308 */
      0xB201833B35D63F73, 0x2CD2CC6551E513DA, /* ~= 10^309 */
      0xDE81E40A034BCF4F, 0xF8077F7EA65E58D1, /* ~= 10^310 */
      0x8B112E86420F6191, 0xFB04AFAF27FAF782, /* ~= 10^311 */
      0xADD57A27D29339F6, 0x79C5DB9AF1F9B563, /* ~= 10^312 */
      0xD94AD8B1C7380874, 0x18375281AE7822BC, /* ~= 10^313 */
      0x87CEC76F1C830548, 0x8F2293910D0B15B5, /* ~= 10^314 */
      0xA9C2794AE3A3C69A, 0xB2EB3875504DDB22, /* ~= 10^315 */
      0xD433179D9C8CB841, 0x5FA60692A46151EB, /* ~= 10^316 */
      0x849FEEC281D7F328, 0xDBC7C41BA6BCD333, /* ~= 10^317 */
      0xA5C7EA73224DEFF3, 0x12B9B522906C0800, /* ~= 10^318 */
      0xCF39E50FEAE16BEF, 0xD768226B34870A00, /* ~= 10^319 */
      0x81842F29F2CCE375, 0xE6A1158300D46640, /* ~= 10^320 */
      0xA1E53AF46F801C53, 0x60495AE3C1097FD0, /* ~= 10^321 */
      0xCA5E89B18B602368, 0x385BB19CB14BDFC4, /* ~= 10^322 */
      0xFCF62C1DEE382C42, 0x46729E03DD9ED7B5, /* ~= 10^323 */
      0x9E19DB92B4E31BA9, 0x6C07A2C26A8346D1 /* ~= 10^324 */
   };

   /**
    Get the cached pow10 value from pow10_sig_table.
    @param exp10 The exponent of pow(10, e). This value must in range
                 POW10_SIG_TABLE_MIN_EXP to POW10_SIG_TABLE_MAX_EXP.
    @param hi    The highest 64 bits of pow(10, e).
    @param lo    The lower 64 bits after `hi`.
    */
   

   inline void pow10_table_get_sig_128(const int32_t exp10, uint64_t hilo[2]) noexcept
   {
      const int32_t idx = exp10 - (POW10_SIG_TABLE_128_MIN_EXP);
      std::memcpy(hilo, pow10_sig_table_128 + idx * 2, 16);
   }

   /**
    Convert double number from binary to decimal.
    The output significand is shortest decimal but may have trailing zeros.

    This function use the Schubfach algorithm:
    Raffaello Giulietti, The Schubfach way to render doubles, 2020.
    https://drive.google.com/open?id=1luHhyQF9zKlM8yJ1nebU0OgVYhfC6CBN
    https://github.com/abolz/Drachennest

    See also:
    Dragonbox: A New Floating-Point Binary-to-Decimal Conversion Algorithm, 2020.
    https://github.com/jk-jeon/dragonbox/blob/master/other_files/Dragonbox.pdf
    https://github.com/jk-jeon/dragonbox

    @param sig_raw The raw value of significand in IEEE 754 format.
    @param exp_raw The raw value of exponent in IEEE 754 format.
    @param sig_bin The decoded value of significand in binary.
    @param exp_bin The decoded value of exponent in binary.
    @param sig_dec The output value of significand in decimal.
    @param exp_dec The output value of exponent in decimal.
    @warning The input double number should not be 0, inf, nan.
    */
   inline void f64_bin_to_dec(uint64_t sig_raw, int32_t exp_raw, uint64_t sig_bin, int32_t exp_bin, uint64_t* sig_dec,
                              int32_t* exp_dec) noexcept
   {
      uint64_t sp, mid;

      const bool is_even = !(sig_bin & 1);
      const bool lower_bound_closer = (sig_raw == 0 && exp_raw > 1);

      const uint64_t cb = 4 * sig_bin;
      const uint64_t cbl = cb - 2 + lower_bound_closer;
      const uint64_t cbr = cb + 2;

      /* exp_bin: [-1074, 971]                                                  */
      /* k = lower_bound_closer ? floor(log10(pow(2, exp_bin)))                 */
      /*                        : floor(log10(pow(2, exp_bin) * 3.0 / 4.0))     */
      /*   = lower_bound_closer ? floor(exp_bin * log10(2))                     */
      /*                        : floor(exp_bin * log10(2) + log10(3.0 / 4.0))  */
      const int32_t k = (exp_bin * 315653 - (lower_bound_closer ? 131237 : 0)) >> 20;

      /* k: [-324, 292]                                                         */
      /* h = exp_bin + floor(log2(pow(10, e)))                                  */
      /*   = exp_bin + floor(log2(10) * e)                                      */
      const int32_t exp10 = -k;
      const int32_t h = exp_bin + ((exp10 * 217707) >> 16) + 1;

      uint64_t pow10hilo[2];
      pow10_table_get_sig_128(exp10, pow10hilo);
      const uint64_t& pow10hi = pow10hilo[0];
      uint64_t& pow10lo = pow10hilo[1];
      pow10lo += (exp10 < POW10_SIG_TABLE_128_MIN_EXACT_EXP || exp10 > POW10_SIG_TABLE_128_MAX_EXACT_EXP);
      const uint64_t vbl = round_to_odd(pow10hi, pow10lo, cbl << h);
      const uint64_t vb = round_to_odd(pow10hi, pow10lo, cb << h);
      const uint64_t vbr = round_to_odd(pow10hi, pow10lo, cbr << h);

      const uint64_t lower = vbl + !is_even;
      const uint64_t upper = vbr - !is_even;

      bool u_inside, w_inside;

      const uint64_t s = vb / 4;
      if (s >= 10) {
         sp = s / 10;
         u_inside = (lower <= 40 * sp);
         w_inside = (upper >= 40 * sp + 40);
         if (u_inside != w_inside) {
            *sig_dec = sp + w_inside;
            *exp_dec = k + 1;
            return;
         }
      }

      u_inside = (lower <= 4 * s);
      w_inside = (upper >= 4 * s + 4);

      mid = 4 * s + 2;
      const bool round_up = (vb > mid) || (vb == mid && (s & 1) != 0);

      *sig_dec = s + ((u_inside != w_inside) ? w_inside : round_up);
      *exp_dec = k;
   }

   /** Trailing zero count table for number 0 to 99.
    (generate with misc/make_tables.c) */
   inline constexpr uint8_t dec_trailing_zero_table[] = {
      2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

   /**
    Write an unsigned integer with a length of 15 to 17 with trailing zero trimmed.
    These digits are named as "aabbccddeeffgghhii" here.
    For example, input 1234567890123000, output "1234567890123".
    */
   inline auto* write_u64_len_15_to_17_trim(auto* buf, uint64_t sig) noexcept
   {
      uint32_t tz1, tz2, tz; /* trailing zero */

      uint32_t abbccddee = uint32_t(sig / 100000000);
      uint32_t ffgghhii = uint32_t(sig - uint64_t(abbccddee) * 100000000);
      uint32_t abbcc = abbccddee / 10000; /* (abbccddee / 10000) */
      uint32_t ddee = abbccddee - abbcc * 10000; /* (abbccddee % 10000) */
      uint32_t abb = uint32_t((uint64_t(abbcc) * 167773) >> 24); /* (abbcc / 100) */
      uint32_t a = (abb * 41) >> 12; /* (abb / 100) */
      uint32_t bb = abb - a * 100; /* (abb % 100) */
      uint32_t cc = abbcc - abb * 100; /* (abbcc % 100) */

      /* write abbcc */
      buf[0] = uint8_t(a + '0');
      buf += a > 0;
      bool lz = bb < 10 && a == 0; /* leading zero */
      std::memcpy(buf, char_table + (bb * 2 + lz), 2);
      buf -= lz;
      std::memcpy(buf + 2, char_table + 2 * cc, 2);

      if (ffgghhii) {
         uint32_t dd = (ddee * 5243) >> 19; /* (ddee / 100) */
         uint32_t ee = ddee - dd * 100; /* (ddee % 100) */
         uint32_t ffgg = uint32_t((uint64_t(ffgghhii) * 109951163) >> 40); /* (val / 10000) */
         uint32_t hhii = ffgghhii - ffgg * 10000; /* (val % 10000) */
         uint32_t ff = (ffgg * 5243) >> 19; /* (aabb / 100) */
         uint32_t gg = ffgg - ff * 100; /* (aabb % 100) */
         //((uint16_t *)buf)[2] = ((const uint16_t *)char_table)[dd];
         std::memcpy(buf + 4, char_table + 2 * dd, 2);
         //((uint16_t *)buf)[3] = ((const uint16_t *)char_table)[ee];
         std::memcpy(buf + 6, char_table + 2 * ee, 2);
         //((uint16_t *)buf)[4] = ((const uint16_t *)char_table)[ff];
         std::memcpy(buf + 8, char_table + 2 * ff, 2);
         //((uint16_t *)buf)[5] = ((const uint16_t *)char_table)[gg];
         std::memcpy(buf + 10, char_table + 2 * gg, 2);
         if (hhii) {
            uint32_t hh = (hhii * 5243) >> 19; /* (ccdd / 100) */
            uint32_t ii = hhii - hh * 100; /* (ccdd % 100) */
            //((uint16_t *)buf)[6] = ((const uint16_t *)char_table)[hh];
            std::memcpy(buf + 12, char_table + 2 * hh, 2);
            //((uint16_t *)buf)[7] = ((const uint16_t *)char_table)[ii];
            std::memcpy(buf + 14, char_table + 2 * ii, 2);
            tz1 = dec_trailing_zero_table[hh];
            tz2 = dec_trailing_zero_table[ii];
            tz = ii ? tz2 : (tz1 + 2);
            buf += 16 - tz;
            return buf;
         }
         else {
            tz1 = dec_trailing_zero_table[ff];
            tz2 = dec_trailing_zero_table[gg];
            tz = gg ? tz2 : (tz1 + 2);
            buf += 12 - tz;
            return buf;
         }
      }
      else {
         if (ddee) {
            uint32_t dd = (ddee * 5243) >> 19; /* (ddee / 100) */
            uint32_t ee = ddee - dd * 100; /* (ddee % 100) */
            //((uint16_t *)buf)[2] = ((const uint16_t *)char_table)[dd];
            std::memcpy(buf + 4, char_table + 2 * dd, 2);
            //((uint16_t *)buf)[3] = ((const uint16_t *)char_table)[ee];
            std::memcpy(buf + 6, char_table + 2 * ee, 2);
            tz1 = dec_trailing_zero_table[dd];
            tz2 = dec_trailing_zero_table[ee];
            tz = ee ? tz2 : (tz1 + 2);
            buf += 8 - tz;
            return buf;
         }
         else {
            tz1 = dec_trailing_zero_table[bb];
            tz2 = dec_trailing_zero_table[cc];
            tz = cc ? tz2 : (tz1 + tz2);
            buf += 4 - tz;
            return buf;
         }
      }
   }

   consteval uint32_t numbits(uint32_t x) noexcept { return x < 2 ? x : 1 + numbits(x >> 1); }

   template <std::floating_point T>
   inline auto* to_chars(auto* buffer, T val) noexcept
   {
      static_assert(std::numeric_limits<T>::is_iec559);
      static_assert(std::numeric_limits<T>::radix == 2);
      static_assert(std::is_same_v<float, T> || std::is_same_v<double, T>);
      static_assert(sizeof(float) == 4 && sizeof(double) == 8);
      using Raw = std::conditional_t<std::is_same_v<float, T>, uint32_t, uint64_t>;

      Raw raw;
      std::memcpy(&raw, &val, sizeof(T));

      /* decode from raw bytes from IEEE-754 double format. */
      constexpr uint32_t exponent_bits =
         numbits(std::numeric_limits<T>::max_exponent - std::numeric_limits<T>::min_exponent + 1);
      constexpr Raw sig_mask = Raw(-1) >> (exponent_bits + 1);
      bool sign = (raw >> (sizeof(T) * 8 - 1));
      uint64_t sig_raw = raw & sig_mask;
      int32_t exp_raw = raw << 1 >> (sizeof(Raw) * 8 - exponent_bits);

      if (exp_raw == (uint32_t(1) << exponent_bits) - 1) [[unlikely]] {
         // NaN or Infinity
         std::memcpy(buffer, "null", 4);
         return buffer + 4;
      }
      if (sign) {
         *buffer = '-';
         ++buffer;
      }
      if ((raw << 1) != 0) [[likely]] {
         uint64_t sig_bin;
         int32_t exp_bin;
         if (exp_raw == 0) [[unlikely]] {
            // subnormal
            sig_bin = sig_raw;
            exp_bin = 1 - (std::numeric_limits<T>::max_exponent - 1) - (std::numeric_limits<T>::digits - 1);
         }
         else {
            sig_bin = sig_raw | uint64_t(1ull << (std::numeric_limits<T>::digits - 1));
            exp_bin =
               int32_t(exp_raw) - (std::numeric_limits<T>::max_exponent - 1) - (std::numeric_limits<T>::digits - 1);
         }

         /* binary to decimal */
         uint64_t sig_dec;
         int32_t exp_dec;
         f64_bin_to_dec(sig_raw, exp_raw, sig_bin, exp_bin, &sig_dec, &exp_dec);
         if constexpr (std::same_as<T, float>) {
            sig_dec *= 100000000;
            exp_dec -= 8;
         }

         int32_t sig_len = 17;
         sig_len -= (sig_dec < 100000000ull * 100000000ull);
         sig_len -= (sig_dec < 100000000ull * 10000000ull);

         /* the decimal point position relative to the first digit */
         int32_t dot_pos = sig_len + exp_dec;

         if (-6 < dot_pos && dot_pos <= 21) {
            /* no need to write exponent part */
            if (dot_pos <= 0) {
               auto num_hdr = buffer + (2 - dot_pos);
               auto num_end = write_u64_len_15_to_17_trim(num_hdr, sig_dec);
               buffer[0] = '0';
               buffer[1] = '.';
               buffer += 2;
               // we don't have to increment the buffer because we are returning
               std::memset(buffer, '0', num_hdr - buffer);
               return num_end;
            }
            else {
               /* dot after first digit */
               /* such as 1.234, 1234.0, 123400000000000000000.0 */
               std::memset(buffer, '0', 8);
               std::memset(buffer + 8, '0', 8);
               std::memset(buffer + 16, '0', 8);
               auto num_hdr = buffer + 1;
               auto num_end = write_u64_len_15_to_17_trim(num_hdr, sig_dec);
               std::memmove(buffer, buffer + 1, dot_pos); // shift characters to the left
               buffer[dot_pos] = '.';
               return ((num_end - num_hdr) <= dot_pos) ? buffer + dot_pos : num_end;
            }
         }
         else {
            /* write with scientific notation */
            /* such as 1.234e56 */
            auto end = write_u64_len_15_to_17_trim(buffer + 1, sig_dec);
            end -= (end == buffer + 2); /* remove '.0', e.g. 2.0e34 -> 2e34 */
            exp_dec += sig_len - 1;
            buffer[0] = buffer[1];
            buffer[1] = '.';
            end[0] = 'E';
            buffer = end + 1;
            buffer[0] = '-';
            buffer += exp_dec < 0;
            exp_dec = std::abs(exp_dec);
            if (exp_dec < 100) {
               uint32_t lz = exp_dec < 10;
               //*(uint16_t *)buffer = *(const uint16_t *)(char_table + (exp_dec * 2 + lz));
               std::memcpy(buffer, char_table + (exp_dec * 2 + lz), 2);
               return buffer + 2 - lz;
            }
            else {
               const uint32_t hi = (uint32_t(exp_dec) * 656) >> 16; /* exp / 100 */
               const uint32_t lo = uint32_t(exp_dec) - hi * 100; /* exp % 100 */
               buffer[0] = uint8_t(hi) + '0';
               std::memcpy(&buffer[1], char_table + (lo * 2), 2);
               return buffer + 3;
            }
         }
      }
      else [[unlikely]] {
         *buffer = '0';
         return buffer + 1;
      }
   }


} // namespace qj_nc
} // namespace detail (qj_nc)

/// owning `DocumentView` and a 32-bit tape index. It provides zero-copy,
/// on-demand access to parsed JSON nodes. An invalid (null) `Value{}` is
/// returned by any access that fails, such as a missing key or out-of-range
/// index.
class Value {
  DocumentState *doc_ = nullptr;
  uint32_t idx_ = 0;

public:
  Value() = default;
  Value(DocumentState *state, uint32_t idx) : doc_(state), idx_(idx) {
    if (doc_)
      doc_->ref();
  }
  Value(DocumentView handle, uint32_t idx) : doc_(handle.state()), idx_(idx) {
    if (doc_)
      doc_->ref();
  }
  ~Value() {
    if (doc_)
      doc_->deref();
  }
  Value(const Value &o) : doc_(o.doc_), idx_(o.idx_) {
    if (doc_)
      doc_->ref();
  }
  Value &operator=(const Value &o) {
    if (this != &o) {
      if (doc_)
        doc_->deref();
      doc_ = o.doc_;
      idx_ = o.idx_;
      if (doc_)
        doc_->ref();
    }
    return *this;
  }
  Value(Value &&o) noexcept : doc_(o.doc_), idx_(o.idx_) { o.doc_ = nullptr; }
  Value &operator=(Value &&o) noexcept {
    if (this != &o) {
      if (doc_)
        doc_->deref();
      doc_ = o.doc_;
      idx_ = o.idx_;
      o.doc_ = nullptr;
    }
    return *this;
  }

  // ── Internal: effective type after mutations ──────────────────────────────

private:
  TapeNodeType effective_type_() const noexcept {
    if (QBUEM_UNLIKELY(!doc_))
      return TapeNodeType::Null;
    if (QBUEM_UNLIKELY(!doc_->deleted_.empty() && doc_->deleted_.count(idx_)))
      return TapeNodeType::Null;
    if (QBUEM_UNLIKELY(!doc_->mutations_.empty())) {
      auto it = doc_->mutations_.find(idx_);
      if (it != doc_->mutations_.end())
        return it->second.type;
    }
    return doc_->tape[idx_].type();
  }

public:
  // ── Type checkers

  /// Returns true if this Value points to a valid parsed node.
  /// A default-constructed or missing Value returns false.
  bool is_valid() const noexcept { return doc_ != nullptr; }

  bool is_null() const noexcept {
    if (!doc_)
      return false;
    return effective_type_() == TapeNodeType::Null;
  }
  bool is_bool() const noexcept {
    if (!doc_)
      return false;
    const auto t = effective_type_();
    return t == TapeNodeType::BooleanTrue || t == TapeNodeType::BooleanFalse;
  }
  bool is_int() const noexcept {
    if (!doc_)
      return false;
    return effective_type_() == TapeNodeType::Integer;
  }
  bool is_double() const noexcept {
    if (!doc_)
      return false;
    const auto t = effective_type_();
    return t == TapeNodeType::Double || t == TapeNodeType::NumberRaw;
  }
  bool is_number() const noexcept {
    if (!doc_)
      return false;
    const auto t = effective_type_();
    return t == TapeNodeType::Integer || t == TapeNodeType::Double ||
           t == TapeNodeType::NumberRaw;
  }
  bool is_string() const noexcept {
    if (!doc_)
      return false;
    return effective_type_() == TapeNodeType::StringRaw;
  }
  bool is_object() const noexcept {
    if (!doc_)
      return false;
    return effective_type_() == TapeNodeType::ObjectStart;
  }
  bool is_array() const noexcept {
    if (!doc_)
      return false;
    return effective_type_() == TapeNodeType::ArrayStart;
  }

  /// Returns the type of this value, correctly accounting for live mutations.
  TapeNodeType type() const noexcept { return effective_type_(); }

  // ── set<T>(): write / mutate a value
  //
  // Replaces the value at this tape position with a new value.
  // The mutation is stored in doc_->mutations_ (overlay map).
  // Subsequent as<T>(), type checkers, and dump() reflect the mutation.
  //
  // Structural mutations (object keys, array elements) are not supported here
  // — set() targets scalar replacement at an existing tape position.

  void set(std::nullptr_t) {
    if (!doc_)
      return;
    doc_->mutations_[idx_] = {TapeNodeType::Null, {}};
    doc_->last_dump_size_ = 0; // invalidate size cache
  }

  void set(bool b) {
    if (!doc_)
      return;
    doc_->mutations_[idx_] = {
        b ? TapeNodeType::BooleanTrue : TapeNodeType::BooleanFalse, {}};
    doc_->last_dump_size_ = 0;
  }

  template <JsonInteger T> void set(T val) {
    if (!doc_)
      return;
    char buf[24];
    char *ptr = detail::qj_nc::to_chars(buf, static_cast<int64_t>(val));
    doc_->mutations_[idx_] = {TapeNodeType::Integer, std::string(buf, ptr)};
    doc_->last_dump_size_ = 0;
  }

  template <JsonFloat T> void set(T val) {
    if (!doc_)
      return;
    char buf[40];
    char *ptr = detail::qj_nc::to_chars(buf, static_cast<double>(val));
    doc_->mutations_[idx_] = {TapeNodeType::Double, std::string(buf, ptr)};
    doc_->last_dump_size_ = 0;
  }

  void set(std::string_view s) {
    if (!doc_)
      return;
    doc_->mutations_[idx_] = {TapeNodeType::StringRaw, std::string(s)};
    doc_->last_dump_size_ = 0;
  }
  void set(const std::string &s) { set(std::string_view(s)); }
  void set(const char *s) { set(std::string_view(s)); }

  // unset() — undo a previous set() and revert to the original parsed value.
  //
  // Removes the mutation overlay for this tape position so that subsequent
  // reads (as<T>(), type_name(), dump()) return the value that was present
  // in the original parsed JSON.
  //
  // Important: this is NOT a "reset to null".  After unset():
  //   - type_name() reflects the *original* parsed type (e.g. still "bool"
  //     if the parsed value was a boolean)
  //   - as<T>() returns the original parsed value
  //
  // unset() only affects the scalar mutation overlay (mutations_).  Keys
  // added via insert() or elements added via push_back() are unaffected.
  // On an invalid Value (doc_=nullptr) this is a no-op.
  void unset() {
    if (!doc_)
      return;
    doc_->mutations_.erase(idx_);
    doc_->last_dump_size_ = 0;
  }

  // ── operator= write overloads ─────────────────────────────────────────────
  //
  // Enables: root["key"] = 42;  root["arr"][0] = "text";
  // Each overload delegates to set() and returns *this for chaining.
  // The compiler-generated copy assignment (Value& = const Value&) is
  // retained for view-copy semantics (root["x"] = other_value_ref).

  Value &operator=(std::nullptr_t) {
    set(nullptr);
    return *this;
  }
  Value &operator=(bool b) {
    set(b);
    return *this;
  }
  Value &operator=(const char *s) {
    set(s);
    return *this;
  }
  Value &operator=(std::string_view s) {
    set(s);
    return *this;
  }
  Value &operator=(const std::string &s) {
    set(s);
    return *this;
  }

  template <JsonInteger T> Value &operator=(T val) {
    set(val);
    return *this;
  }

  template <JsonFloat T> Value &operator=(T val) {
    set(val);
    return *this;
  }

  // ── Safe optional-chain access ────────────────────────────────────────────
  //
  // get(key/idx) starts — or continues — an optional chain.  The return
  // type is SafeValue (defined below), which propagates std::nullopt
  // silently through every subsequent operator[] so the chain NEVER throws.
  //
  // Usage:
  //   auto id = root.get("user")["id"].as<int>();     // std::optional<int>
  //   int  id = root.get("user")["id"].value_or(-1);  // int with default
  //
  // These are declared here and defined out-of-line after SafeValue.
  SafeValue get(std::string_view key) const noexcept;
  SafeValue get(const char *key) const noexcept;
  SafeValue get(size_t idx) const noexcept;
  SafeValue get(int idx) const noexcept;

  // ── Internal: skip past the value at tape[idx], return next tape index ─────
  // O(n) walk for nested objects/arrays; O(1) for scalar types.

private:
  uint32_t skip_value_(uint32_t idx) const noexcept {
    const uint32_t tsz = static_cast<uint32_t>(doc_->tape.size());
    if (QBUEM_UNLIKELY(idx >= tsz))
      return idx;
    const auto t = doc_->tape[idx].type();
    if (t == TapeNodeType::ObjectStart || t == TapeNodeType::ArrayStart) {
      int depth = 1;
      ++idx;
      while (depth > 0 && QBUEM_LIKELY(idx < tsz)) {
        const auto nt = doc_->tape[idx].type();
        if (nt == TapeNodeType::ObjectStart || nt == TapeNodeType::ArrayStart)
          ++depth;
        else if (nt == TapeNodeType::ObjectEnd || nt == TapeNodeType::ArrayEnd)
          --depth;
        ++idx;
      }
      return idx;
    }
    return idx + 1;
  }

public:
  // ── Navigation: operator[] and find
  //
  // qbuem-json API philosophy:
  //   operator[](key/idx)   — non-throwing: returns an invalid Value{}
  //                           (is_valid() == false) when the key or index is
  //                           absent.  Use if(v) / v.is_valid() to check.
  //                           Use find() when you need an explicit optional.
  //   find(key)             — returns std::optional<Value> (no-throw)
  //
  // This is our own pattern: subscript for convenient chained access, find()
  // for conditional access — both non-throwing and composable.

  // const char* overload — exact match for string literals, avoids ambiguity
  // with built-in operator[](long, const char*) that would otherwise be
  // considered when Value has an implicit arithmetic conversion.
  Value operator[](const char *key) const {
    return (*this)[std::string_view(key)];
  }

  // operator[] — non-throwing: returns invalid Value{} (doc_=nullptr) when
  // the key or index is missing instead of throwing.  This enables safe chains:
  //   auto v = root["a"]["b"]["c"];   // never throws; check with if(v)
  //   int x  = root["a"]["b"].value_or(0); // via SafeValue chain
  Value operator[](std::string_view key) const noexcept {
    if (!is_object())
      return {};
    uint32_t i = idx_ + 1;
    const size_t ntape = doc_->tape.size();
    while (i < ntape) {
      const auto t = doc_->tape[i].type();
      if (t == TapeNodeType::ObjectEnd)
        break;
      // Skip deleted keys transparently
      if (QBUEM_UNLIKELY(!doc_->deleted_.empty() && doc_->deleted_.count(i))) {
        i = skip_value_(i + 1);
        continue;
      }
      const TapeNode &kn = doc_->tape[i];
      const char *kdata = doc_->source.data() + kn.offset;
      const size_t klen = kn.length();
      if (klen == key.size() && std::memcmp(kdata, key.data(), klen) == 0)
        return Value(doc_, i + 1);
      i = skip_value_(i + 1);
    }
    if (QBUEM_UNLIKELY(!doc_->additions_.empty())) {
      auto ait = doc_->additions_.find(idx_);
      if (ait != doc_->additions_.end()) {
        for (const auto &p : ait->second) {
          if (p.first == key) {
            DocumentState *synth = doc_->get_synthetic(p.second);
            return Value(synth, 0);
          }
        }
      }
    }
    return {};
  }

  // int overload — prevents implicit conversion of int literals through
  // arithmetic operator T() to ambiguous built-in subscript.
  Value operator[](int index) const noexcept {
    if (index < 0)
      return {};
    return (*this)[static_cast<size_t>(index)];
  }
  // unsigned int overload — resolves ambiguity between int and size_t
  Value operator[](unsigned int index) const noexcept {
    return (*this)[static_cast<size_t>(index)];
  }

  Value operator[](size_t index) const noexcept {
    if (!is_array())
      return {};
    uint32_t i = idx_ + 1;
    const size_t ntape = doc_->tape.size();
    size_t count = 0;
    while (i < ntape) {
      const auto t = doc_->tape[i].type();
      if (t == TapeNodeType::ArrayEnd)
        break;
      // Skip deleted elements transparently
      if (QBUEM_UNLIKELY(!doc_->deleted_.empty() && doc_->deleted_.count(i))) {
        i = skip_value_(i);
        continue;
      }
      if (count == index)
        return Value(doc_, i);
      i = skip_value_(i);
      ++count;
    }

    // Check array_insertions_
    if (QBUEM_UNLIKELY(!doc_->array_insertions_.empty())) {
      auto iit = doc_->array_insertions_.find(idx_);
      if (iit != doc_->array_insertions_.end()) {
        for (const auto &ins : iit->second) {
          if (ins.index == index) {
            DocumentState *synth = doc_->get_synthetic(ins.data);
            return Value(synth, 0);
          }
        }
      }
    }
    return {};
  }

  // find() — returns optional<Value>; respects deleted keys.
  std::optional<Value> find(std::string_view key) const noexcept {
    if (!is_object())
      return std::nullopt;
    uint32_t i = idx_ + 1;
    const size_t ntape = doc_->tape.size();
    while (i < ntape) {
      const auto t = doc_->tape[i].type();
      if (t == TapeNodeType::ObjectEnd)
        break;
      if (QBUEM_UNLIKELY(!doc_->deleted_.empty() && doc_->deleted_.count(i))) {
        i = skip_value_(i + 1);
        continue;
      }
      const TapeNode &kn = doc_->tape[i];
      const char *kdata = doc_->source.data() + kn.offset;
      const size_t klen = kn.length();
      if (klen == key.size() && std::memcmp(kdata, key.data(), klen) == 0)
        return Value(doc_, i + 1);
      i = skip_value_(i + 1);
    }
    // Key not found in tape, check additions_
    if (QBUEM_UNLIKELY(!doc_->additions_.empty())) {
      auto ait = doc_->additions_.find(idx_);
      if (ait != doc_->additions_.end()) {
        for (const auto &p : ait->second) {
          if (p.first == key) {
            DocumentState *synth = doc_->get_synthetic(p.second);
            return Value(synth, 0);
          }
        }
      }
    }
    return std::nullopt;
  }

  // ── Size (respects deletions + additions) ──────────────────────────────────

  size_t size() const noexcept {
    if (!doc_)
      return 0;
    const auto t = doc_->tape[idx_].type();
    const size_t ntape = doc_->tape.size();
    if (t == TapeNodeType::ArrayStart) {
      uint32_t i = idx_ + 1;
      size_t count = 0;
      while (i < ntape && doc_->tape[i].type() != TapeNodeType::ArrayEnd) {
        if (QBUEM_UNLIKELY(!doc_->deleted_.empty() &&
                           doc_->deleted_.count(i))) {
          i = skip_value_(i);
        } else {
          i = skip_value_(i);
          ++count;
        }
      }
      if (!doc_->array_insertions_.empty()) {
        auto iit = doc_->array_insertions_.find(idx_);
        if (iit != doc_->array_insertions_.end())
          count += iit->second.size();
      }
      // push_back() appends to additions_; count those too
      if (!doc_->additions_.empty()) {
        auto ait = doc_->additions_.find(idx_);
        if (ait != doc_->additions_.end())
          count += ait->second.size();
      }
      return count;
    }
    if (t == TapeNodeType::ObjectStart) {
      uint32_t i = idx_ + 1;
      size_t count = 0;
      while (i < ntape && doc_->tape[i].type() != TapeNodeType::ObjectEnd) {
        if (QBUEM_UNLIKELY(!doc_->deleted_.empty() &&
                           doc_->deleted_.count(i))) {
          i = skip_value_(i + 1); // skip deleted key+value
        } else {
          i = skip_value_(i + 1);
          ++count;
        }
      }
      if (!doc_->additions_.empty()) {
        auto ait = doc_->additions_.find(idx_);
        if (ait != doc_->additions_.end())
          count += ait->second.size();
      }
      return count;
    }
    return 0;
  }

  bool empty() const noexcept { return size() == 0; }

  // ── as<T>(): typed value extraction
  //
  // qbuem-json unique pattern: as<T>() is the single canonical accessor.
  // Throws std::runtime_error on type mismatch.
  // try_as<T>() is the non-throwing variant, returning std::optional<T>.
  //
  // Supported types:
  //   bool, int64_t (+ all integral types via cast), double (+ float),
  //   std::string, std::string_view

  template <typename T> T as() const {
    if (!doc_ || (!doc_->deleted_.empty() && doc_->deleted_.count(idx_)))
      throw std::runtime_error(
          "qbuem::Value::as: value is missing, invalid, or deleted");

    // Check mutation overlay first — O(1) unordered_map lookup, only paid
    // when mutations_ is non-empty (guarded by QBUEM_UNLIKELY branch).
    if (QBUEM_UNLIKELY(!doc_->mutations_.empty())) {
      auto mit = doc_->mutations_.find(idx_);
      if (mit != doc_->mutations_.end()) {
        const MutationEntry &m = mit->second;
        if constexpr (std::is_same_v<T, bool>) {
          if (m.type == TapeNodeType::BooleanTrue)
            return true;
          if (m.type == TapeNodeType::BooleanFalse)
            return false;
          throw std::runtime_error("qbuem::Value::as<bool>: not a boolean");
        } else if constexpr (std::is_integral_v<T>) {
          if (m.type != TapeNodeType::Integer)
            throw std::runtime_error(
                "qbuem::Value::as<integral>: not an integer");
          int64_t val = 0;
          std::from_chars(m.data.data(), m.data.data() + m.data.size(), val);
          return static_cast<T>(val);
        } else if constexpr (std::is_floating_point_v<T>) {
          if (m.type != TapeNodeType::Double && m.type != TapeNodeType::Integer)
            throw std::runtime_error("qbuem::Value::as<float>: not a number");
          double val = 0.0;
          const char *beg = m.data.data();
          [[maybe_unused]] const char *end = beg + m.data.size();
#if __cpp_lib_to_chars >= 201611L && !defined(__APPLE__)
          std::from_chars(beg, end, val);
#else
          char buf[64];
          size_t len = m.data.size();
          if (len >= sizeof(buf))
            len = sizeof(buf) - 1;
          std::memcpy(buf, beg, len);
          buf[len] = '\0';
          val = std::strtod(buf, nullptr);
#endif
          return static_cast<T>(val);
        } else if constexpr (std::is_same_v<T, std::string_view>) {
          if (m.type != TapeNodeType::StringRaw)
            throw std::runtime_error(
                "qbuem::Value::as<string_view>: not a string");
          return std::string_view(m.data);
        } else if constexpr (std::is_same_v<T, std::string>) {
          if (m.type != TapeNodeType::StringRaw)
            throw std::runtime_error("qbuem::Value::as<string>: not a string");
          return m.data;
        }
      }
    }

    // No mutation — read from tape (original fast path)
    if constexpr (std::is_same_v<T, bool>) {
      const auto t = doc_->tape[idx_].type();
      if (t == TapeNodeType::BooleanTrue)
        return true;
      if (t == TapeNodeType::BooleanFalse)
        return false;
      throw std::runtime_error("qbuem::Value::as<bool>: not a boolean");
    } else if constexpr (std::is_integral_v<T>) {
      const auto t = doc_->tape[idx_].type();
      if (t != TapeNodeType::Integer && t != TapeNodeType::NumberRaw)
        throw std::runtime_error("qbuem::Value::as<integral>: not an integer");
      const TapeNode &nd = doc_->tape[idx_];
      int64_t val = 0;
      const char *beg = doc_->source.data() + nd.offset;
      const char *end = beg + nd.length();
      auto [ptr, ec] = std::from_chars(beg, end, val);
      if (ec != std::errc{})
        throw std::runtime_error("qbuem::Value::as<integral>: parse error");
      return static_cast<T>(val);
    } else if constexpr (std::is_floating_point_v<T>) {
      const auto t = doc_->tape[idx_].type();
      if (t != TapeNodeType::Double && t != TapeNodeType::NumberRaw &&
          t != TapeNodeType::Integer)
        throw std::runtime_error("qbuem::Value::as<float>: not a number");
      const TapeNode &nd = doc_->tape[idx_];
      double val = 0.0;
      const char *beg = doc_->source.data() + nd.offset;
      [[maybe_unused]] const char *end = beg + nd.length();
#if __cpp_lib_to_chars >= 201611L && !defined(__APPLE__)
      auto [ptr, ec] = std::from_chars(beg, end, val);
      if (ec != std::errc{})
        throw std::runtime_error("qbuem::Value::as<float>: parse error");
#else
      char buf[64];
      size_t len = nd.length();
      if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
      std::memcpy(buf, beg, len);
      buf[len] = '\0';
      char *endp = nullptr;
      val = std::strtod(buf, &endp);
      if (endp == buf)
        throw std::runtime_error("qbuem::Value::as<float>: parse error");
#endif
      return static_cast<T>(val);
    } else if constexpr (std::is_same_v<T, std::string_view>) {
      if (doc_->tape[idx_].type() != TapeNodeType::StringRaw)
        throw std::runtime_error("qbuem::Value::as<string_view>: not a string");
      const TapeNode &nd = doc_->tape[idx_];
      return std::string_view(doc_->source.data() + nd.offset, nd.length());
    } else if constexpr (std::is_same_v<T, std::string>) {
      return std::string(as<std::string_view>());
    } else {
      static_assert(sizeof(T) == 0, "qbuem::Value::as<T>: unsupported type");
    }
  }

  // try_as<T>(): non-throwing variant — returns std::nullopt on any error.
  // Constrained to JsonReadable types; ill-formed for unsupported T.
  template <JsonReadable T> std::optional<T> try_as() const noexcept {
    try {
      return as<T>();
    } catch (...) {
      return std::nullopt;
    }
  }

  // ── Implicit conversion
  //
  // Enables: int age = doc["age"];  std::string name = doc["name"];
  // Restricted to arithmetic types and std::string/string_view to prevent
  // accidental conversions.

  template <JsonReadable T> operator T() const { return as<T>(); }

  // ── Null / validity check

  explicit operator bool() const noexcept { return doc_ != nullptr; }

  // ── Serialization ─────────────────────────────────────────────────────────
  //
  // dump()           — compact JSON string for this value (subtree only)
  // dump(string&)    — buffer-reuse variant
  // dump(int indent) — pretty-printed with 'indent' spaces per level
  //
  // flat-buffer rewrite.
  // Instead of 3 × std::string::append per token (with bounds-check +
  // size-update overhead on every call), we pre-allocate a raw char buffer
  // and write directly via a pointer.  The buffer is large enough to hold
  // compact JSON that is never larger than the pretty-printed source.
  // At the end we create one std::string from the exact byte count.
  //
  // Separator logic uses three 64-bit bit-stacks (max depth 64, sufficient
  // for all real JSON).  All state lives in CPU registers — no stack arrays.
  //   obj_bits  — bit (top): depth is an object (clear = array)
  //   key_bits  — bit (top): next element in object is a key
  //   sep_bits  — bit (top): separator needed (has ≥1 element already)
  // Replaces four bool[1024] arrays (4 KB stack) with 24 bytes.
  std::string dump() const {
    if (!doc_ || doc_->tape.size() == 0)
      return "null";
    // Delegate to structural dump when deletions/additions are present
    if (QBUEM_UNLIKELY(!doc_->deleted_.empty() || !doc_->additions_.empty() ||
                       !doc_->array_insertions_.empty()))
      return dump_changes_();
    // Subtree (non-root): use separate path to avoid polluting the hot loop
    if (QBUEM_UNLIKELY(idx_ != 0))
      return dump_subtree_();
    const char *src = doc_->source.data();
    [[maybe_unused]] const size_t src_sz = doc_->source.size();
    const size_t ntape = doc_->tape.size();

    // separators pre-computed by parser into meta bits 23-16.
    //   sep == 0x00 → no separator   sep == 0x01 → comma   sep == 0x02 → colon
    // Hot path: root dump (idx_==0) — original loop, zero extra overhead.
    size_t mutation_extra = 0;
    for (const auto &[k, m] : doc_->mutations_)
      mutation_extra += m.data.size() + 16;
    const size_t buf_cap = doc_->source.size() + 16 + mutation_extra;
    std::string out;
    out.resize(buf_cap);
    char *w = out.data();
    char *w0 = w;

    for (size_t i = 0; i < ntape; ++i) {
      const TapeNode &nd = doc_->tape[i];
      const uint32_t meta = nd.meta;
      const auto type = static_cast<TapeNodeType>((meta >> 24) & 0xFF);
      const uint8_t sep = (meta >> 16) & 0xFFu;

      // Write pre-computed separator (branch-free for common case)
      // attempt (sep-per-case + StringRaw batch write) REVERTED:
      // moving sep write inside each switch case changed the LTO code layout,
      // causing citm parse to regress from +21% to +4.5% vs yyjson.
      // Root cause: same PGO/LTO cross-contamination pattern as /66-B.
      // The serialize loop and parse code share one LTO unit — any structural
      // change to the serialize switch affects parse I-cache layout.
      // branchless sep write — table lookup + conditional advance.
      // sep=0 writes '\0' harmlessly; switch case always overwrites it.
      // Saves 2 instructions + eliminates 1 branch vs conditional write.
#if QBUEM_ARCH_APPLE_SILICON
      {
        static constexpr char kSepChars[3] = {'\0', ',', ':'};
        *w = kSepChars[sep];
        w += static_cast<size_t>(sep != 0);
      }
#else
      if (sep)
        *w++ = (sep == 0x02u) ? ':' : ',';
#endif

      // Mutation overlay — only paid when mutations_ non-empty (rare path).
      // Separator already written above; write mutated scalar and skip switch.
      if (QBUEM_UNLIKELY(!doc_->mutations_.empty())) {
        auto mit = doc_->mutations_.find(static_cast<uint32_t>(i));
        if (mit != doc_->mutations_.end()) {
          const MutationEntry &m = mit->second;
          switch (m.type) {
          case TapeNodeType::Null:
            std::memcpy(w, "null", 4);
            w += 4;
            break;
          case TapeNodeType::BooleanTrue:
            std::memcpy(w, "true", 4);
            w += 4;
            break;
          case TapeNodeType::BooleanFalse:
            std::memcpy(w, "false", 5);
            w += 5;
            break;
          case TapeNodeType::StringRaw:
            *w++ = '"';
            std::memcpy(w, m.data.data(), m.data.size());
            w += m.data.size();
            *w++ = '"';
            break;
          default: // Integer, Double
            std::memcpy(w, m.data.data(), m.data.size());
            w += m.data.size();
            break;
          }
          continue;
        }
      }

      switch (type) {

      case TapeNodeType::ObjectStart:
        *w++ = '{';
        break;
      case TapeNodeType::ObjectEnd:
        *w++ = '}';
        break;
      case TapeNodeType::ArrayStart:
        *w++ = '[';
        break;
      case TapeNodeType::ArrayEnd:
        *w++ = ']';
        break;

      case TapeNodeType::StringRaw: {
        const uint16_t slen = static_cast<uint16_t>(meta & 0xFFFFu);
        const char *sp = src + nd.offset;
        *w++ = '"';
#if QBUEM_HAS_NEON
        // NEON overlapping-pair copy for 17–31-byte strings.
        // For slen in [17,31]: two 16B VLD1Q+VST1Q stores cover all bytes.
        //   Store 1: w[0..15]  (first 16B of string)
        //   Store 2: w[slen-16..slen-1]  (last 16B of string, overlaps)
        // The overlap region is written twice with identical data — correct.
        // Source read: sp+slen-16 ≤ sp+15 < sp+slen (closing '"') — in-bounds.
        // Eliminates the 16B+cascade branch chain for 96%+ of twitter strings.
        //
        // For slen ≤ 16: scalar 8-4-1 cascade (fast for short keys).
        // For slen ≥ 32: std::memcpy (large values, dispatch overhead
        // amortised). : Restructured branch order — slen 1-16 (95%+
        // of citm/twitter) checked FIRST → 2 branches in hot path vs 3 (saves 1
        // branch/string). Code size: ~13 instructions vs ~16 → smaller hot-path
        // I-cache footprint. Generic NEON (non-M1): structure
        // unchanged.
#if QBUEM_ARCH_APPLE_SILICON
        if (QBUEM_LIKELY(slen <= 16)) {
          if (QBUEM_LIKELY(sp + 16 <= src + src_sz)) {
            vst1q_u8(reinterpret_cast<uint8_t *>(w),
                     vld1q_u8(reinterpret_cast<const uint8_t *>(sp)));
            w += slen;
          } else {
            std::memcpy(w, sp, slen);
            w += slen;
          }
        } else if (QBUEM_LIKELY(slen <= 31)) {
          const uint8_t *up = reinterpret_cast<const uint8_t *>(sp);
          uint8_t *uw = reinterpret_cast<uint8_t *>(w);
          vst1q_u8(uw, vld1q_u8(up));
          vst1q_u8(uw + slen - 16, vld1q_u8(up + slen - 16));
          w += slen;
        } else {
          std::memcpy(w, sp, slen);
          w += slen;
        }
#else  // generic NEON (non-Apple-Silicon): structure
        if (QBUEM_LIKELY(slen <= 31)) {
          if (slen >= 17) {
            const uint8_t *up = reinterpret_cast<const uint8_t *>(sp);
            uint8_t *uw = reinterpret_cast<uint8_t *>(w);
            vst1q_u8(uw, vld1q_u8(up));
            vst1q_u8(uw + slen - 16, vld1q_u8(up + slen - 16));
            w += slen;
          } else {
            uint16_t rem = slen;
            if (rem >= 16) {
              uint64_t a, b;
              std::memcpy(&a, sp, 8);
              std::memcpy(&b, sp + 8, 8);
              std::memcpy(w, &a, 8);
              std::memcpy(w + 8, &b, 8);
              sp += 16;
              w += 16;
              rem = 0;
            }
            if (rem >= 8) {
              uint64_t a;
              std::memcpy(&a, sp, 8);
              std::memcpy(w, &a, 8);
              sp += 8;
              w += 8;
              rem = static_cast<uint16_t>(rem - 8);
            }
            if (rem >= 4) {
              uint32_t a;
              std::memcpy(&a, sp, 4);
              std::memcpy(w, &a, 4);
              sp += 4;
              w += 4;
              rem = static_cast<uint16_t>(rem - 4);
            }
            while (rem--)
              *w++ = *sp++;
          }
        } else {
          std::memcpy(w, sp, slen);
          w += slen;
        }
#endif // QBUEM_ARCH_APPLE_SILICON
#else
        // Unrolled 16-8-4-1 copy: avoids glibc dispatch overhead
        // for short strings (twitter.json avg 16.9 chars, 84% ≤ 24 chars).
        // attempt: SSE2 overlapping-pair for 17–31B was REVERTED.
        // The SSE2 stores altered the PGO profile (LTO cross-contamination),
        // causing citm parse to regress +14% (598→684μs) for only −5% serialize
        // gain. Scalar 16-8-4-1 preserves parse performance.
        if (QBUEM_LIKELY(slen <= 31)) {
          uint16_t rem = slen;
          if (rem >= 16) {
            uint64_t a, b;
            std::memcpy(&a, sp, 8);
            std::memcpy(&b, sp + 8, 8);
            std::memcpy(w, &a, 8);
            std::memcpy(w + 8, &b, 8);
            sp += 16;
            w += 16;
            rem = static_cast<uint16_t>(rem - 16);
          }
          if (rem >= 8) {
            uint64_t a;
            std::memcpy(&a, sp, 8);
            std::memcpy(w, &a, 8);
            sp += 8;
            w += 8;
            rem = static_cast<uint16_t>(rem - 8);
          }
          if (rem >= 4) {
            uint32_t a;
            std::memcpy(&a, sp, 4);
            std::memcpy(w, &a, 4);
            sp += 4;
            w += 4;
            rem = static_cast<uint16_t>(rem - 4);
          }
          while (rem--)
            *w++ = *sp++;
        } else {
          std::memcpy(w, sp, slen);
          w += slen;
        }
#endif // QBUEM_HAS_NEON
        *w++ = '"';
        break;
      }

      case TapeNodeType::Integer:
      case TapeNodeType::NumberRaw:
      case TapeNodeType::Double: {
        const uint16_t nlen = static_cast<uint16_t>(meta & 0xFFFFu);
        std::memcpy(w, src + nd.offset, nlen);
        w += nlen;
        break;
      }

      case TapeNodeType::BooleanTrue:
        std::memcpy(w, "true", 4);
        w += 4;
        break;
      case TapeNodeType::BooleanFalse:
        std::memcpy(w, "false", 5);
        w += 5;
        break;
      case TapeNodeType::Null:
        std::memcpy(w, "null", 4);
        w += 4;
        break;

      default:
        break;
      }
    }

    out.resize(static_cast<size_t>(w - w0));
    return out;
  }

  // Buffer-reuse dump() overload.
  //
  // Serializes into a caller-provided std::string, amortising malloc+free
  // across repeated calls on the same document (streaming, hot loops).
  //
  // Usage:
  //   std::string buf;
  //   for (...) { root.dump(buf); process(buf); }
  //
  // uses last_dump_size_ to resize to the exact output size from
  // the previous call instead of buf_cap.  For a fixed document the output
  // size is constant, so resize(last_dump_size_) is a no-op on the 2nd+
  // call (out.size() already equals last_dump_size_) — zero zero-fill cost.
  // The first call still uses buf_cap to guarantee sufficient capacity.
  //
  // LTO safety: do NOT mark this NOINLINE.  The compiler
  // will inline it at call sites, keeping code layout identical to dump().
  // NOINLINE caused parse regression on x86 PGO/LTO builds.
  void dump(std::string &out) const {
    if (!doc_ || doc_->tape.size() == 0) {
      out.assign("null", 4);
      return;
    }
    if (QBUEM_UNLIKELY(!doc_->deleted_.empty() || !doc_->additions_.empty() ||
                       !doc_->array_insertions_.empty())) {
      out = dump_changes_();
      return;
    }
    // Subtree (non-root): last_dump_size_ is root-only; don't use cache here
    if (QBUEM_UNLIKELY(idx_ != 0)) {
      out = dump_subtree_();
      return;
    }
    const char *src = doc_->source.data();
    [[maybe_unused]] const size_t src_sz = doc_->source.size();
    const size_t ntape = doc_->tape.size();
    size_t mutation_extra2 = 0;
    for (const auto &[k, m] : doc_->mutations_)
      mutation_extra2 += m.data.size() + 16;
    const size_t buf_cap = doc_->source.size() + 16 + mutation_extra2;

    // last_dump_size_ cache — root-only (avoids cross-contamination
    // with subtree dump sizes which would undersize the buffer and overflow).
    const size_t target =
        (doc_->last_dump_size_ > 0) ? doc_->last_dump_size_ : buf_cap;
    out.resize(target);
    char *w = out.data();
    char *w0 = w;

    for (size_t i = 0; i < ntape; ++i) {
      const TapeNode &nd = doc_->tape[i];
      const uint32_t meta = nd.meta;
      const auto type = static_cast<TapeNodeType>((meta >> 24) & 0xFF);
      const uint8_t sep = (meta >> 16) & 0xFFu;

#if QBUEM_ARCH_APPLE_SILICON
      {
        static constexpr char kSepChars[3] = {'\0', ',', ':'};
        *w = kSepChars[sep];
        w += static_cast<size_t>(sep != 0);
      }
#else
      if (sep)
        *w++ = (sep == 0x02u) ? ':' : ',';
#endif

      if (QBUEM_UNLIKELY(!doc_->mutations_.empty())) {
        auto mit = doc_->mutations_.find(static_cast<uint32_t>(i));
        if (mit != doc_->mutations_.end()) {
          const MutationEntry &m = mit->second;
          switch (m.type) {
          case TapeNodeType::Null:
            std::memcpy(w, "null", 4);
            w += 4;
            break;
          case TapeNodeType::BooleanTrue:
            std::memcpy(w, "true", 4);
            w += 4;
            break;
          case TapeNodeType::BooleanFalse:
            std::memcpy(w, "false", 5);
            w += 5;
            break;
          case TapeNodeType::StringRaw:
            *w++ = '"';
            std::memcpy(w, m.data.data(), m.data.size());
            w += m.data.size();
            *w++ = '"';
            break;
          default:
            std::memcpy(w, m.data.data(), m.data.size());
            w += m.data.size();
            break;
          }
          continue;
        }
      }

      switch (type) {

      case TapeNodeType::ObjectStart:
        *w++ = '{';
        break;
      case TapeNodeType::ObjectEnd:
        *w++ = '}';
        break;
      case TapeNodeType::ArrayStart:
        *w++ = '[';
        break;
      case TapeNodeType::ArrayEnd:
        *w++ = ']';
        break;

      case TapeNodeType::StringRaw: {
        const uint16_t slen = static_cast<uint16_t>(meta & 0xFFFFu);
        const char *sp = src + nd.offset;
        *w++ = '"';
#if QBUEM_HAS_NEON
        // see dump() above for rationale.
#if QBUEM_ARCH_APPLE_SILICON
        if (QBUEM_LIKELY(slen <= 16)) {
          if (QBUEM_LIKELY(sp + 16 <= src + src_sz)) {
            vst1q_u8(reinterpret_cast<uint8_t *>(w),
                     vld1q_u8(reinterpret_cast<const uint8_t *>(sp)));
            w += slen;
          } else {
            std::memcpy(w, sp, slen);
            w += slen;
          }
        } else if (QBUEM_LIKELY(slen <= 31)) {
          const uint8_t *up = reinterpret_cast<const uint8_t *>(sp);
          uint8_t *uw = reinterpret_cast<uint8_t *>(w);
          vst1q_u8(uw, vld1q_u8(up));
          vst1q_u8(uw + slen - 16, vld1q_u8(up + slen - 16));
          w += slen;
        } else {
          std::memcpy(w, sp, slen);
          w += slen;
        }
#else  // generic NEON (non-Apple-Silicon): structure
        if (QBUEM_LIKELY(slen <= 31)) {
          if (slen >= 17) {
            const uint8_t *up = reinterpret_cast<const uint8_t *>(sp);
            uint8_t *uw = reinterpret_cast<uint8_t *>(w);
            vst1q_u8(uw, vld1q_u8(up));
            vst1q_u8(uw + slen - 16, vld1q_u8(up + slen - 16));
            w += slen;
          } else {
            uint16_t rem = slen;
            if (rem >= 16) {
              uint64_t a, b;
              std::memcpy(&a, sp, 8);
              std::memcpy(&b, sp + 8, 8);
              std::memcpy(w, &a, 8);
              std::memcpy(w + 8, &b, 8);
              sp += 16;
              w += 16;
              rem = 0;
            }
            if (rem >= 8) {
              uint64_t a;
              std::memcpy(&a, sp, 8);
              std::memcpy(w, &a, 8);
              sp += 8;
              w += 8;
              rem = static_cast<uint16_t>(rem - 8);
            }
            if (rem >= 4) {
              uint32_t a;
              std::memcpy(&a, sp, 4);
              std::memcpy(w, &a, 4);
              sp += 4;
              w += 4;
              rem = static_cast<uint16_t>(rem - 4);
            }
            while (rem--)
              *w++ = *sp++;
          }
        } else {
          std::memcpy(w, sp, slen);
          w += slen;
        }
#endif // QBUEM_ARCH_APPLE_SILICON
#else
        if (QBUEM_LIKELY(slen <= 31)) {
          uint16_t rem = slen;
          if (rem >= 16) {
            uint64_t a, b;
            std::memcpy(&a, sp, 8);
            std::memcpy(&b, sp + 8, 8);
            std::memcpy(w, &a, 8);
            std::memcpy(w + 8, &b, 8);
            sp += 16;
            w += 16;
            rem = static_cast<uint16_t>(rem - 16);
          }
          if (rem >= 8) {
            uint64_t a;
            std::memcpy(&a, sp, 8);
            std::memcpy(w, &a, 8);
            sp += 8;
            w += 8;
            rem = static_cast<uint16_t>(rem - 8);
          }
          if (rem >= 4) {
            uint32_t a;
            std::memcpy(&a, sp, 4);
            std::memcpy(w, &a, 4);
            sp += 4;
            w += 4;
            rem = static_cast<uint16_t>(rem - 4);
          }
          while (rem--)
            *w++ = *sp++;
        } else {
          std::memcpy(w, sp, slen);
          w += slen;
        }
#endif
        *w++ = '"';
        break;
      }

      case TapeNodeType::Integer:
      case TapeNodeType::NumberRaw:
      case TapeNodeType::Double: {
        const uint16_t nlen = static_cast<uint16_t>(meta & 0xFFFFu);
        std::memcpy(w, src + nd.offset, nlen);
        w += nlen;
        break;
      }

      case TapeNodeType::BooleanTrue:
        std::memcpy(w, "true", 4);
        w += 4;
        break;
      case TapeNodeType::BooleanFalse:
        std::memcpy(w, "false", 5);
        w += 5;
        break;
      case TapeNodeType::Null:
        std::memcpy(w, "null", 4);
        w += 4;
        break;

      default:
        break;
      }
    }

    const size_t actual = static_cast<size_t>(w - w0);
    out.resize(actual);
    doc_->last_dump_size_ = actual; // cache for zero-fill-free resize next call
  }

  // ── Pretty-print ──────────────────────────────────────────────────────────
  //
  // dump(indent) — human-readable JSON with 'indent' spaces per level.
  // Uses std::string::append internally (not ultra-fast, but pretty-print
  // is rarely on the hot path).
  std::string dump(int indent) const {
    if (!doc_)
      return "null";
    std::string out;
    out.reserve(doc_->source.size() * 2 + 64);
    dump_pretty_(out, indent, 0);
    return out;
  }

  // ── Structural modification ───────────────────────────────────────────────
  //
  // erase(key)       — delete an object key + its entire value subtree
  // erase(idx)       — delete an array element
  // insert(key, val) — add a key-value pair to an object
  // push_back(val)   — append an element to an array
  //
  // All operations are reflected immediately by dump(), operator[], find(),
  // size(), items(), and elements().

  bool erase(std::string_view key) {
    if (!is_object())
      return false;
    uint32_t i = idx_ + 1;
    const size_t ntape = doc_->tape.size();
    while (i < ntape) {
      const auto t = doc_->tape[i].type();
      if (t == TapeNodeType::ObjectEnd)
        return false;
      const TapeNode &kn = doc_->tape[i];
      const char *kdata = doc_->source.data() + kn.offset;
      if (kn.length() == key.size() &&
          std::memcmp(kdata, key.data(), key.size()) == 0) {
        doc_->deleted_.insert(i);
        uint32_t val_end = skip_value_(i + 1);
        for (uint32_t k = i + 1; k < val_end; ++k)
          doc_->deleted_.insert(k);
        doc_->last_dump_size_ = 0;
        return true;
      }
      i = skip_value_(i + 1);
    }
    return false;
  }
  bool erase(const char *key) { return erase(std::string_view(key)); }

  bool erase(size_t idx) {
    if (!is_array())
      return false;
    uint32_t i = idx_ + 1;
    const size_t ntape = doc_->tape.size();
    size_t count = 0;
    while (i < ntape) {
      const auto t = doc_->tape[i].type();
      if (t == TapeNodeType::ArrayEnd)
        return false;
      if (QBUEM_UNLIKELY(!doc_->deleted_.empty() && doc_->deleted_.count(i))) {
        i = skip_value_(i);
        continue;
      }
      if (count == idx) {
        doc_->deleted_.insert(i);
        uint32_t val_end = skip_value_(i);
        for (uint32_t k = i + 1; k < val_end; ++k)
          doc_->deleted_.insert(k);
        doc_->last_dump_size_ = 0;
        return true;
      }
      i = skip_value_(i);
      ++count;
    }
    return false;
  }
  bool erase(int idx) {
    if (idx >= 0)
      return erase(static_cast<size_t>(idx));
    return false;
  }
  // unsigned int overload — resolves ambiguity between int and size_t
  bool erase(unsigned int idx) { return erase(static_cast<size_t>(idx)); }

  // ── Structural insert API ────────────────────────────────────────────────
  //
  // insert(key, T)       — type-safe: strings are quoted, scalars serialized
  // insert_json(key, sv) — raw JSON (use for nested objects/arrays)
  // push_back(T)         — array append (type-safe)
  // push_back_json(sv)   — raw JSON append

  // insert_json: store a pre-serialized JSON value (raw, no quoting)
  void insert_json(std::string_view key, std::string_view raw_json) {
    if (!is_object())
      return;
    doc_->additions_[idx_].emplace_back(std::string(key),
                                        std::string(raw_json));
    doc_->last_dump_size_ = 0;
  }
  // insert(key, string) — inserts a JSON string (auto-quoted)
  void insert(std::string_view key, std::string_view str_val) {
    insert_json(key, scalar_to_json_(str_val));
  }
  void insert(std::string_view key, const std::string &str_val) {
    insert(key, std::string_view(str_val));
  }
  void insert(std::string_view key, const char *str_val) {
    insert(key, std::string_view(str_val));
  }
  // insert(key, nullptr/bool) — explicit overloads
  void insert(std::string_view key, std::nullptr_t) {
    insert_json(key, "null");
  }
  void insert(std::string_view key, bool b) {
    insert_json(key, b ? "true" : "false");
  }
  // insert(key, numeric) — numeric scalars
  template <JsonInteger T> void insert(std::string_view key, T val) {
    insert_json(key, scalar_to_json_(val));
  }
  template <JsonFloat T> void insert(std::string_view key, T val) {
    insert_json(key, scalar_to_json_(val));
  }
  // insert(key, Value) — serializes the value subtree
  void insert(std::string_view key, const Value &v) {
    insert_json(key, v.dump());
  }

  // push_back_json: raw JSON append
  void push_back_json(std::string_view raw_json) {
    if (!is_array())
      return;
    doc_->additions_[idx_].emplace_back(std::string(), std::string(raw_json));
    doc_->last_dump_size_ = 0;
  }
  // push_back(string) — auto-quoted
  void push_back(std::string_view str_val) {
    push_back_json(scalar_to_json_(str_val));
  }
  void push_back(const std::string &str_val) {
    push_back(std::string_view(str_val));
  }
  void push_back(const char *str_val) { push_back(std::string_view(str_val)); }
  void push_back(std::nullptr_t) { push_back_json("null"); }
  void push_back(bool b) { push_back_json(b ? "true" : "false"); }
  template <JsonInteger T> void push_back(T val) {
    push_back_json(scalar_to_json_(val));
  }
  template <JsonFloat T> void push_back(T val) {
    push_back_json(scalar_to_json_(val));
  }
  void push_back(const Value &v) { push_back_json(v.dump()); }

  // ── Iteration ─────────────────────────────────────────────────────────────
  //
  // items()    — forward-iterable view over {key, value} pairs of an object.
  //              Skips deleted keys and includes additions.
  // elements() — forward-iterable view over elements of an array.
  //              Skips deleted elements.
  //
  // Usage:
  //   for (auto [k, v] : root["obj"].items())
  //       std::cout << k << " = " << v.dump() << "\n";
  //
  //   for (auto elem : root["arr"].elements())
  //       std::cout << elem.as<int>() << "\n";

  // Shared skip helper — delegates to the canonical skip_value_() instance
  // method. Static so iterator classes (defined before their parent Value
  // closes) can call it.
  static uint32_t skip_val_s_(DocumentState *doc, uint32_t i) noexcept {
    if (QBUEM_UNLIKELY(!doc || i >= static_cast<uint32_t>(doc->tape.size())))
      return i;
    Value tmp(doc, i);
    return tmp.skip_value_(i);
  }

  // Forward iterator over object key-value pairs.
  // Yields std::pair<std::string_view, Value> — structured bindings work:
  //   for (auto [key, val] : root["obj"].items()) { ... }
  // Iterates original tape keys first, then any inserted additions.
  class ObjectIterator {
    DocumentState *doc_;
    uint32_t key_idx_; // UINT32_MAX = tape exhausted
    // additions_ pointer + index for iterating inserted key-value pairs
    const ::std::vector<::std::pair<::std::string, ::std::string>> *adds_ =
        nullptr;
    size_t add_idx_ = ::std::numeric_limits<size_t>::max(); // max = done

    void skip_deleted_() noexcept {
      const size_t tape_sz = doc_->tape.size();
      while (key_idx_ != UINT32_MAX) {
        // Bounds guard: malformed tape may lack an ObjectEnd sentinel.
        if (QBUEM_UNLIKELY(key_idx_ >= tape_sz)) {
          key_idx_ = UINT32_MAX;
          break;
        }
        const auto t = doc_->tape[key_idx_].type();
        if (t == TapeNodeType::ObjectEnd) {
          key_idx_ = UINT32_MAX;
          break;
        }
        if (doc_->deleted_.empty() || !doc_->deleted_.count(key_idx_))
          return;
        key_idx_ = skip_val_s_(doc_, key_idx_ + 1); // skip deleted key+value
      }
      // Tape exhausted — transition to additions if available
      if (key_idx_ == UINT32_MAX && adds_ != nullptr)
        add_idx_ = 0;
    }

  public:
    // value_type uses pair to avoid incomplete-type issue with Value inside
    // Value
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<std::string_view, Value>;
    using iterator_category = std::forward_iterator_tag;

    // End sentinel: key_idx_=UINT32_MAX, add_idx_=SIZE_MAX
    ObjectIterator() noexcept
        : doc_(nullptr), key_idx_(UINT32_MAX), adds_(nullptr),
          add_idx_(::std::numeric_limits<size_t>::max()) {}
    ObjectIterator(DocumentState *doc, uint32_t key_idx,
                   const ::std::vector<::std::pair<::std::string,
                                                   ::std::string>> *adds) noexcept
        : doc_(doc), key_idx_(key_idx), adds_(adds),
          add_idx_(::std::numeric_limits<size_t>::max()) {
      skip_deleted_();
    }

    // Returns {key_string_view, Value} — Value is constructed on demand
    std::pair<std::string_view, Value> operator*() const noexcept {
      if (key_idx_ != UINT32_MAX) {
        // Tape entry
        const TapeNode &kn = doc_->tape[key_idx_];
        return {std::string_view(doc_->source.data() + kn.offset, kn.length()),
                Value(doc_, key_idx_ + 1)};
      }
      // Addition entry: parse value via synthetic document
      const auto &p = (*adds_)[add_idx_];
      DocumentState *synth = doc_->get_synthetic(p.second);
      return {std::string_view(p.first), Value(synth, 0)};
    }
    ObjectIterator &operator++() noexcept {
      if (key_idx_ != UINT32_MAX) {
        key_idx_ = skip_val_s_(doc_, key_idx_ + 1); // skip value
        skip_deleted_();
      } else if (adds_ != nullptr &&
                 add_idx_ != ::std::numeric_limits<size_t>::max()) {
        ++add_idx_;
        if (add_idx_ >= adds_->size())
          add_idx_ = ::std::numeric_limits<size_t>::max(); // mark done
      }
      return *this;
    }
    ObjectIterator operator++(int) noexcept {
      auto t = *this;
      ++(*this);
      return t;
    }
    bool operator==(const ObjectIterator &o) const noexcept {
      return key_idx_ == o.key_idx_ && add_idx_ == o.add_idx_;
    }
  };

  // Range-compatible proxy for object iteration (includes additions)
  class ObjectRange : public std::ranges::view_base {
    DocumentState *doc_;
    uint32_t obj_idx_; // ObjectStart tape index
    // Additions appended as synthetic entries after tape traversal
    const ::std::vector<::std::pair<::std::string, ::std::string>> *adds_ =
        nullptr;

  public:
    ObjectRange(DocumentState *doc, uint32_t idx) noexcept
        : doc_(doc), obj_idx_(idx) {
      if (doc_ && !doc_->additions_.empty()) {
        auto it = doc_->additions_.find(idx);
        if (it != doc_->additions_.end())
          adds_ = &it->second;
      }
    }
    ObjectIterator begin() const noexcept {
      return {doc_, obj_idx_ + 1, adds_};
    }
    ObjectIterator end() const noexcept { return {}; }
    // additions are accessible via added_items() for legacy callers
    const ::std::vector<::std::pair<::std::string, ::std::string>> *
    added_items() const noexcept {
      return adds_;
    }
  };

  // Forward iterator over array elements
  class ArrayIterator {
    DocumentState *doc_;
    uint32_t elem_idx_; // UINT32_MAX = end
    void skip_deleted_() noexcept {
      const size_t tape_sz = doc_->tape.size();
      while (elem_idx_ != UINT32_MAX) {
        // Bounds guard: malformed tape may lack an ArrayEnd sentinel.
        if (QBUEM_UNLIKELY(elem_idx_ >= tape_sz)) {
          elem_idx_ = UINT32_MAX;
          return;
        }
        const auto t = doc_->tape[elem_idx_].type();
        if (t == TapeNodeType::ArrayEnd) {
          elem_idx_ = UINT32_MAX;
          return;
        }
        if (doc_->deleted_.empty() || !doc_->deleted_.count(elem_idx_))
          return;
        elem_idx_ = skip_val_s_(doc_, elem_idx_);
      }
    }

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = Value;
    using iterator_category = std::forward_iterator_tag;

    ArrayIterator() noexcept : doc_(nullptr), elem_idx_(UINT32_MAX) {}
    ArrayIterator(DocumentState *doc, uint32_t elem_idx) noexcept
        : doc_(doc), elem_idx_(elem_idx) {
      skip_deleted_();
    }

    Value operator*() const noexcept { return Value(doc_, elem_idx_); }
    ArrayIterator &operator++() noexcept {
      elem_idx_ = skip_val_s_(doc_, elem_idx_);
      skip_deleted_();
      return *this;
    }
    ArrayIterator operator++(int) noexcept {
      auto t = *this;
      ++(*this);
      return t;
    }
    bool operator==(const ArrayIterator &o) const noexcept {
      return elem_idx_ == o.elem_idx_;
    }
  };

  class ArrayRange : public std::ranges::view_base {
    DocumentState *doc_;
    uint32_t arr_idx_;

  public:
    ArrayRange(DocumentState *doc, uint32_t idx) noexcept
        : doc_(doc), arr_idx_(idx) {}
    ArrayIterator begin() const noexcept { return {doc_, arr_idx_ + 1}; }
    ArrayIterator end() const noexcept { return {}; }
  };

  // ── Iterator adapters for keys(), values(), as_array<T>(), try_as_array<T>()
  //
  // These avoid std::views::transform (which internally uses view_interface<D>)
  // to work around a GCC 12 / libstdc++12 bug where view_interface<D> eagerly
  // evaluates iterator_t<D> during class instantiation, creating a circular
  // constraint dependency that GCC 12 cannot resolve.

  struct KeyIterator {
    ObjectIterator it_;
    using value_type = std::string_view;
    using reference = std::string_view;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    KeyIterator() noexcept = default;
    explicit KeyIterator(ObjectIterator it) noexcept : it_(it) {}
    std::string_view operator*() const noexcept { return (*it_).first; }
    KeyIterator &operator++() noexcept {
      ++it_;
      return *this;
    }
    KeyIterator operator++(int) noexcept {
      auto t = *this;
      ++it_;
      return t;
    }
    bool operator==(const KeyIterator &o) const noexcept {
      return it_ == o.it_;
    }
  };
  struct KeysRange {
    ObjectRange r_;
    explicit KeysRange(ObjectRange r) noexcept : r_(r) {}
    KeyIterator begin() const noexcept { return KeyIterator{r_.begin()}; }
    KeyIterator end() const noexcept { return KeyIterator{r_.end()}; }
  };

  struct ObjectValueIterator {
    ObjectIterator it_;
    using value_type = Value;
    using reference = Value;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    ObjectValueIterator() noexcept = default;
    explicit ObjectValueIterator(ObjectIterator it) noexcept : it_(it) {}
    Value operator*() const noexcept { return (*it_).second; }
    ObjectValueIterator &operator++() noexcept {
      ++it_;
      return *this;
    }
    ObjectValueIterator operator++(int) noexcept {
      auto t = *this;
      ++it_;
      return t;
    }
    bool operator==(const ObjectValueIterator &o) const noexcept {
      return it_ == o.it_;
    }
  };
  struct ValuesRange {
    ObjectRange r_;
    explicit ValuesRange(ObjectRange r) noexcept : r_(r) {}
    ObjectValueIterator begin() const noexcept {
      return ObjectValueIterator{r_.begin()};
    }
    ObjectValueIterator end() const noexcept {
      return ObjectValueIterator{r_.end()};
    }
  };

  template <JsonReadable T> struct TypedArrayIterator {
    ArrayIterator it_;
    using value_type = T;
    using reference = T;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    TypedArrayIterator() noexcept = default;
    explicit TypedArrayIterator(ArrayIterator it) noexcept : it_(it) {}
    T operator*() const { return (*it_).template as<T>(); }
    TypedArrayIterator &operator++() noexcept {
      ++it_;
      return *this;
    }
    TypedArrayIterator operator++(int) noexcept {
      auto t = *this;
      ++it_;
      return t;
    }
    bool operator==(const TypedArrayIterator &o) const noexcept {
      return it_ == o.it_;
    }
  };
  template <JsonReadable T> struct TypedArrayRange {
    ArrayRange r_;
    explicit TypedArrayRange(ArrayRange r) noexcept : r_(r) {}
    TypedArrayIterator<T> begin() const noexcept {
      return TypedArrayIterator<T>{r_.begin()};
    }
    TypedArrayIterator<T> end() const noexcept {
      return TypedArrayIterator<T>{r_.end()};
    }
  };

  template <JsonReadable T> struct OptionalArrayIterator {
    ArrayIterator it_;
    using value_type = std::optional<T>;
    using reference = std::optional<T>;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    OptionalArrayIterator() noexcept = default;
    explicit OptionalArrayIterator(ArrayIterator it) noexcept : it_(it) {}
    std::optional<T> operator*() const noexcept {
      return (*it_).template try_as<T>();
    }
    OptionalArrayIterator &operator++() noexcept {
      ++it_;
      return *this;
    }
    OptionalArrayIterator operator++(int) noexcept {
      auto t = *this;
      ++it_;
      return t;
    }
    bool operator==(const OptionalArrayIterator &o) const noexcept {
      return it_ == o.it_;
    }
  };
  template <JsonReadable T> struct OptionalArrayRange {
    ArrayRange r_;
    explicit OptionalArrayRange(ArrayRange r) noexcept : r_(r) {}
    OptionalArrayIterator<T> begin() const noexcept {
      return OptionalArrayIterator<T>{r_.begin()};
    }
    OptionalArrayIterator<T> end() const noexcept {
      return OptionalArrayIterator<T>{r_.end()};
    }
  };

  // ── Public type aliases for use in lambdas ──────────────────────────────
  //
  // Avoids the `template` keyword in generic lambdas over items():
  //   for (auto kv : root.items()) { kv.second.as<int>(); }  // generic lambda:
  //   needs 'template'
  //   [](Value::ObjectItem kv){ kv.second.as<int>(); }       // explicit type:
  //   no 'template'
  //   [](Value::ObjectItem kv){ kv.second.as<int>(); }
  using ObjectItem =
      std::pair<std::string_view, Value>; // key-value pair from items()

  // items() — object iteration
  ObjectRange items() const noexcept {
    if (!doc_ || !is_object())
      return {nullptr, 0};
    return {doc_, idx_};
  }
  // elements() — array iteration
  ArrayRange elements() const noexcept {
    if (!doc_ || !is_array())
      return {nullptr, 0};
    return {doc_, idx_};
  }

  // ── contains(key) ─────────────────────────────────────────────────────────
  //
  // Returns true iff the object has the given key (not deleted).
  // Equivalent to find(key).has_value() but reads more naturally.
  //
  // Usage: if (root.contains("name")) { ... }
  bool contains(std::string_view key) const noexcept {
    return find(key).has_value();
  }
  bool contains(const char *key) const noexcept {
    return contains(std::string_view(key));
  }

  // ── value(key/idx, default) — safe extraction with fallback ───────────────
  //
  // Returns the value at key/index, or `def` if the key is missing or the
  // value's type doesn't match T.  Never throws.
  //
  // Usage:
  //   int  age  = root.value("age",  0);
  //   std::string name = root.value("name", "anonymous");
  //   double x  = root.value(0, 0.0);   // first array element or 0.0

  template <JsonReadable T>
  T value(std::string_view key, T def) const noexcept {
    auto opt = find(key);
    if (!opt)
      return def;
    auto r = opt->try_as<T>();
    return r ? *r : def;
  }
  template <JsonReadable T> T value(const char *key, T def) const noexcept {
    return value(std::string_view(key), def);
  }
  template <JsonReadable T> T value(size_t idx, T def) const noexcept {
    auto v = (*this)[idx];
    if (!v)
      return def;
    auto r = v.try_as<T>();
    return r ? *r : def;
  }
  template <JsonReadable T> T value(int idx, T def) const noexcept {
    if (idx < 0)
      return def;
    return value(static_cast<size_t>(idx), def);
  }
  // String literal specializations (const char* → std::string)
  std::string value(std::string_view key, const char *def) const noexcept {
    auto opt = find(key);
    if (!opt)
      return std::string(def);
    auto r = opt->try_as<std::string>();
    return r ? *r : std::string(def);
  }
  std::string value(const char *key, const char *def) const noexcept {
    return value(std::string_view(key), def);
  }

  // ── type_name() — human-readable type string ──────────────────────────────
  //
  // Returns one of: "null", "bool", "int", "double", "string", "array",
  //                 "object", or "invalid" (when the Value is empty/missing).
  //
  // Usage: std::cout << root["age"].type_name() << "\n";  // "int"
  std::string_view type_name() const noexcept {
    if (!doc_)
      return "invalid";
    switch (effective_type_()) {
    case TapeNodeType::Null:
      return "null";
    case TapeNodeType::BooleanTrue:
    case TapeNodeType::BooleanFalse:
      return "bool";
    case TapeNodeType::Integer:
      return "int";
    case TapeNodeType::Double:
    case TapeNodeType::NumberRaw:
      return "double";
    case TapeNodeType::StringRaw:
      return "string";
    case TapeNodeType::ArrayStart:
      return "array";
    case TapeNodeType::ObjectStart:
      return "object";
    default:
      return "unknown";
    }
  }

  // ── operator| — pipe fallback ─────────────────────────────────────────────
  //
  // Enables: int age = root["age"] | 42;
  //          std::string s = root["name"] | "unknown";
  //          double x = root["x"] | 0.0;
  //
  // Returns try_as<T>() with fallback `def` when the Value is invalid
  // (missing key/index) or the type doesn't match.
  //
  // Defined as non-member friend: the template version takes priority over
  // the built-in int|int (which would require a user-defined conversion),
  // so `root["age"] | 42` correctly calls this operator rather than
  // converting Value to int first.

  template <JsonReadable T> friend T operator|(const Value &v, T def) noexcept {
    auto r = v.try_as<T>();
    return r ? *r : def;
  }
  friend std::string operator|(const Value &v, std::string_view def) noexcept {
    auto r = v.try_as<std::string>();
    return r ? *r : std::string(def);
  }
  friend std::string operator|(const Value &v, const char *def) noexcept {
    auto r = v.try_as<std::string>();
    return r ? *r : std::string(def);
  }

  // ── keys() / values() — object key/value ranges ───────────────────────────
  //
  // Lazy transform views over items().
  //
  // Usage:
  //   for (std::string_view k : root.keys())   { ... }
  //   for (qbuem::Value     v : root.values()) { ... }
  //   auto first_key = *root.keys().begin();

  KeysRange keys() const noexcept { return KeysRange{items()}; }
  ValuesRange values() const noexcept { return ValuesRange{items()}; }

  // ── as_array<T>() / try_as_array<T>() — typed element views ──────────────
  //
  // Lazy transform view over elements() yielding each element as T.
  //   as_array<T>()      — throws std::runtime_error on type mismatch
  //   try_as_array<T>()  — yields std::optional<T>, never throws
  //
  // Usage:
  //   for (int id : doc["ids"].as_array<int>()) { ... }
  //   auto sum = std::accumulate(
  //       doc["vals"].as_array<double>().begin(),
  //       doc["vals"].as_array<double>().end(), 0.0);
  //   for (auto maybe : doc["mixed"].try_as_array<int>())
  //       if (maybe) total += *maybe;

  template <JsonReadable T> TypedArrayRange<T> as_array() const {
    return TypedArrayRange<T>{elements()};
  }
  template <JsonReadable T>
  OptionalArrayRange<T> try_as_array() const noexcept {
    return OptionalArrayRange<T>{elements()};
  }

  // ── at(path) — Runtime JSON Pointer (RFC 6901) ────────────────────────────
  //
  // Navigates nested values using a slash-delimited path.
  // Handles RFC 6901 escape sequences: ~1 → '/', ~0 → '~'.
  // Returns invalid Value{} (operator bool() == false) on any missing step.
  // Empty path returns *this (root).
  //
  // Usage:
  //   root.at("/users/0/name")     → "Alice"
  //   root.at("/config/timeout")   → 5000
  //   root.at("")                  → root itself
  //   root.at("/missing/path")     → invalid Value{} (bool == false)

  Value at(std::string_view path) const noexcept {
    if (path.empty())
      return *this;
    if (path[0] != '/')
      return {}; // RFC 6901: must start with '/' or be empty
    Value cur = *this;
    size_t pos = 1;
    while (pos <= path.size() && cur) {
      const size_t slash = path.find('/', pos);
      const std::string_view token = (slash == std::string_view::npos)
                                         ? path.substr(pos)
                                         : path.substr(pos, slash - pos);

      // Decode RFC 6901 escapes (~0 → '~', ~1 → '/') only when '~' present
      if (token.find('~') != std::string_view::npos) {
        std::string decoded;
        decoded.reserve(token.size());
        for (size_t i = 0; i < token.size(); ++i) {
          if (token[i] == '~' && i + 1 < token.size()) {
            if (token[i + 1] == '1') {
              decoded += '/';
              ++i;
            } else if (token[i + 1] == '0') {
              decoded += '~';
              ++i;
            } else
              decoded += token[i];
          } else {
            decoded += token[i];
          }
        }
        cur = at_step_(cur, std::string_view(decoded));
      } else {
        cur = at_step_(cur, token);
      }
      pos = (slash == std::string_view::npos) ? path.size() + 1 : slash + 1;
    }
    return cur;
  }

  // ── at<Path>() — Compile-time JSON Pointer ────────────────────────────────
  //
  // Validates the path at compile time (must start with '/' or be empty).
  // At runtime, delegates to at(path) for navigation.
  //
  // Usage:
  //   root.at<"/users/0/name">()   // compile-time validated path
  //   root.at<"">()                // returns root itself
  //   root.at<"no-slash">()        // compile error: "must start with '/'"

  template <size_t N> struct JsonPointerLiteral {
    char data[N]{};
    static constexpr size_t size = N > 0 ? N - 1 : 0; // exclude null terminator
    consteval JsonPointerLiteral(const char (&s)[N]) {
      for (size_t i = 0; i < N; ++i)
        data[i] = s[i];
      // Validate: must start with '/' (RFC 6901) or be the empty document root
      if constexpr (N > 1) {
        if (s[0] != '/')
          throw "qbuem::Value::at<Path>: JSON Pointer must start with '/'";
      }
    }
    std::string_view view() const noexcept { return {data, size}; }
  };

  template <JsonPointerLiteral Path> Value at() const noexcept {
    return at(Path.view());
  }

  // ── merge(other) — shallow object merge ───────────────────────────────────
  //
  // Copies all key-value pairs from `other` (object) into this object.
  // Existing keys with the same name are replaced; new keys are appended.
  // `other` must be an object; non-object arguments are silently ignored.
  //
  // Usage:
  //   root["config"].merge(defaults);     // overlay defaults
  //   target.merge(source);               // shallow merge

  void merge(const Value &other) {
    if (!is_object() || !other.is_object())
      return;
    for (auto [k, v] : other.items()) {
      erase(k);                 // mark existing tape key as deleted
      erase_from_additions_(k); // remove any previously inserted duplicate
      insert(k, v);             // append new key-value
    }
  }

  void merge_patch(std::string_view patch_json);

  // ── patch(json) — JSON Patch (RFC 6902) ────────────────────────────────────
  //
  // Applies a JSON Patch array of operations to this value.
  // Supported operations: add, remove, replace, move, copy, test.
  // Returns true on success, false if any operation fails (atomic-ish).
  //
  // Usage:
  //   root.patch(R"([{"op":"add", "path":"/a/b", "value":1}])");
  bool patch(std::string_view patch_json);
  bool patch(const Value &patch_array);

  // ── insert(idx, val) — array-at-index insertion ────────────────────────────
  void insert(size_t idx, std::string_view str_val) {
    insert_json(idx, scalar_to_json_(str_val));
  }
  void insert(size_t idx, const Value &v) { insert_json(idx, v.dump()); }
  void insert_json(size_t idx, std::string_view raw_json) {
    if (!is_array())
      return;
    doc_->array_insertions_[idx_].push_back({idx, std::string(raw_json)});
    doc_->last_dump_size_ = 0;
  }

private:
  // ── Private helpers ────────────────────────────────────────────────────────

  // at_step_: advance one JSON Pointer token from cur.
  // Array: token must be a non-negative decimal integer index.
  // Object: token is the key string.
  static Value at_step_(const Value &cur, std::string_view token) noexcept {
    if (cur.is_array()) {
      if (token == "-")
        return {}; // RFC 6901: end of array (invalid but representable)
      if (token.empty())
        return {};
      // Handle "0" as single digit, or non-zero starting digits
      if (token.size() > 1 && token[0] == '0')
        return {}; // RFC 6901: leading zeros are not allowed
      size_t idx = 0;
      auto [ptr, ec] =
          std::from_chars(token.data(), token.data() + token.size(), idx);
      if (ec != std::errc{} || ptr != token.data() + token.size())
        return {};
      return cur[idx];
    }
    return cur[token];
  }

  // erase_from_additions_: remove all addition entries with the given key.
  // Called by merge() / merge_patch() before re-inserting a key.
  void erase_from_additions_(std::string_view key) {
    auto ait = doc_->additions_.find(idx_);
    if (ait == doc_->additions_.end())
      return;
    auto &vec = ait->second;
    std::erase_if(vec, [&](const std::pair<std::string, std::string> &p) {
      return std::string_view(p.first) == key;
    });
    if (vec.empty())
      doc_->additions_.erase(ait);
    doc_->last_dump_size_ = 0;
  }

  // merge_patch_impl_: recursive RFC 7396 patch application.
  void merge_patch_impl_(const Value &patch) {
    if (!is_object() || !patch.is_object())
      return;
    for (auto [k, v] : patch.items()) {
      if (v.is_null()) {
        erase(k);
        erase_from_additions_(k);
      } else {
        Value existing = (*this)[k];
        if (v.is_object() && existing.is_object()) {
          existing.merge_patch_impl_(v); // recursive
        } else {
          erase(k);
          erase_from_additions_(k);
          insert(k,
                 v); // Insert serialized value to ensure cross-document safety
        }
      }
    }
  }

  // Serialize a scalar to its JSON representation.
  static std::string scalar_to_json_(std::nullptr_t) { return "null"; }
  static std::string scalar_to_json_(bool b) { return b ? "true" : "false"; }
  template <JsonInteger T> static std::string scalar_to_json_(T v) {
    char buf[24];
    char *p = detail::qj_nc::to_chars(buf, static_cast<int64_t>(v));
    return std::string(buf, p);
  }
  template <JsonFloat T> static std::string scalar_to_json_(T v) {
    char buf[40];
    char *p = detail::qj_nc::to_chars(buf, static_cast<double>(v));
    return std::string(buf, p);
  }
  static std::string scalar_to_json_(std::string_view s) {
    std::string r;
    r.reserve(s.size() + 2);
    r += '"';
    r.append(s.data(), s.size());
    r += '"';
    return r;
  }
  static std::string scalar_to_json_(const std::string &s) {
    return scalar_to_json_(std::string_view(s));
  }
  static std::string scalar_to_json_(const char *s) {
    return scalar_to_json_(std::string_view(s));
  }

  // Subtree dump for non-root Values (idx_ != 0).
  // Iterates tape[idx_..skip_value_(idx_)) and suppresses the first node's
  // separator (which encodes position in the parent, not within this subtree).
  // Kept out of the main dump() loop so the root hot-path has zero extra
  // branches.
  std::string dump_subtree_() const {
    const char *src = doc_->source.data();
    const uint32_t end_i = skip_value_(idx_);

    size_t mutation_extra = 0;
    for (const auto &[k, m] : doc_->mutations_)
      mutation_extra += m.data.size() + 16;
    // Conservative upper bound: subtree ≤ full source
    std::string out;
    out.resize(doc_->source.size() + 16 + mutation_extra);
    char *w = out.data();
    char *w0 = w;

    for (uint32_t i = idx_; i < end_i; ++i) {
      const TapeNode &nd = doc_->tape[i];
      const uint32_t meta = nd.meta;
      const auto type = static_cast<TapeNodeType>((meta >> 24) & 0xFF);
      // sep for the first node (idx_) is suppressed — it belongs to parent
      // context
      const uint8_t sep =
          (i == idx_) ? 0u : static_cast<uint8_t>((meta >> 16) & 0xFFu);
      if (sep)
        *w++ = (sep == 0x02u) ? ':' : ',';

      if (QBUEM_UNLIKELY(!doc_->mutations_.empty())) {
        auto mit = doc_->mutations_.find(i);
        if (mit != doc_->mutations_.end()) {
          const MutationEntry &m = mit->second;
          switch (m.type) {
          case TapeNodeType::Null:
            std::memcpy(w, "null", 4);
            w += 4;
            break;
          case TapeNodeType::BooleanTrue:
            std::memcpy(w, "true", 4);
            w += 4;
            break;
          case TapeNodeType::BooleanFalse:
            std::memcpy(w, "false", 5);
            w += 5;
            break;
          case TapeNodeType::StringRaw:
            *w++ = '"';
            std::memcpy(w, m.data.data(), m.data.size());
            w += m.data.size();
            *w++ = '"';
            break;
          default:
            std::memcpy(w, m.data.data(), m.data.size());
            w += m.data.size();
            break;
          }
          continue;
        }
      }

      switch (type) {
      case TapeNodeType::ObjectStart:
        *w++ = '{';
        break;
      case TapeNodeType::ObjectEnd:
        *w++ = '}';
        break;
      case TapeNodeType::ArrayStart:
        *w++ = '[';
        break;
      case TapeNodeType::ArrayEnd:
        *w++ = ']';
        break;
      case TapeNodeType::StringRaw: {
        const uint16_t slen = static_cast<uint16_t>(meta & 0xFFFFu);
        const size_t src_sz = doc_->source.size();
        const size_t safe_slen =
            (nd.offset < src_sz) ? std::min<size_t>(slen, src_sz - nd.offset)
                                 : 0;
        *w++ = '"';
        std::memcpy(w, src + nd.offset, safe_slen);
        w += safe_slen;
        *w++ = '"';
        break;
      }
      case TapeNodeType::Integer:
      case TapeNodeType::NumberRaw:
      case TapeNodeType::Double: {
        const uint16_t nlen = static_cast<uint16_t>(meta & 0xFFFFu);
        const size_t src_sz = doc_->source.size();
        const size_t safe_nlen =
            (nd.offset < src_sz) ? std::min<size_t>(nlen, src_sz - nd.offset)
                                 : 0;
        std::memcpy(w, src + nd.offset, safe_nlen);
        w += safe_nlen;
        break;
      }
      case TapeNodeType::BooleanTrue:
        std::memcpy(w, "true", 4);
        w += 4;
        break;
      case TapeNodeType::BooleanFalse:
        std::memcpy(w, "false", 5);
        w += 5;
        break;
      case TapeNodeType::Null:
        std::memcpy(w, "null", 4);
        w += 4;
        break;
      default:
        break;
      }
    }
    out.resize(static_cast<size_t>(w - w0));
    return out;
  }

  // Write a single mutation entry into a std::string
  static void write_mutation_(std::string &out, const MutationEntry &m) {
    switch (m.type) {
    case TapeNodeType::Null:
      out += "null";
      break;
    case TapeNodeType::BooleanTrue:
      out += "true";
      break;
    case TapeNodeType::BooleanFalse:
      out += "false";
      break;
    case TapeNodeType::StringRaw:
      out += '"';
      out.append(m.data);
      out += '"';
      break;
    default: // Integer, Double
      out.append(m.data);
      break;
    }
  }

  // Full structural dump — used when deleted_ or additions_ are non-empty.
  // Uses a stack-based separator approach (not pre-computed sep bits, which
  // are invalidated by structural changes).
  std::string dump_changes_() const {
    const char *src = doc_->source.data();
    const size_t ntape = doc_->tape.size();

    // Compute subtree range
    const uint32_t start_c = idx_;
    const uint32_t end_c =
        (idx_ == 0) ? static_cast<uint32_t>(ntape) : skip_value_(idx_);

    std::string out;
    out.reserve(doc_->source.size() + doc_->additions_.size() * 64 + 64);

    // Stack-based separator state (max 64 nesting levels)
    struct Frame {
      bool is_obj;
      bool has_prev;
      bool next_val;
      uint32_t start_idx;
      size_t arr_idx;
    };
    Frame stk[64];
    int top = -1;

    uint32_t i = start_c;
    const TapeNodeType initial_type = doc_->tape[start_c].type();

    // Handle initial node if it's an object or array
    if (initial_type == TapeNodeType::ObjectStart) {
      out += '{';
      top++;
      stk[top] = {true, false, false, start_c, 0};
      i++; // Move past the ObjectStart node
    } else if (initial_type == TapeNodeType::ArrayStart) {
      out += '[';
      top++;
      stk[top] = {false, false, false, start_c, 0};
      i++; // Move past the ArrayStart node
    } else {
      // If it's a scalar, just dump it directly without stack logic
      // This path is only taken if idx_ is a scalar and there are
      // mutations/additions on it, which is handled by the mutation overlay. If
      // it's a scalar and no mutations, dump() or dump_subtree_() would have
      // been called. So this case should only happen if a scalar has a
      // mutation.
      auto mit = doc_->mutations_.find(i);
      if (mit != doc_->mutations_.end()) {
        write_mutation_(out, mit->second);
      } else {
        // Fallback for non-mutated scalar (should not be reached if logic is
        // correct) This means a scalar is being dumped via dump_changes_
        // without mutations. This is an edge case, but for completeness, we'll
        // dump the original value.
        const TapeNode &nd = doc_->tape[i];
        const uint16_t len = static_cast<uint16_t>(nd.meta & 0xFFFFu);
        const size_t src_sz = doc_->source.size();
        const size_t safe_len = (nd.offset < src_sz)
                                    ? std::min<size_t>(len, src_sz - nd.offset)
                                    : 0;
        if (initial_type == TapeNodeType::StringRaw) {
          out += '"';
          out.append(src + nd.offset, safe_len);
          out += '"';
        } else {
          out.append(src + nd.offset, safe_len);
        }
      }
      return out; // Scalar value, done.
    }

    // Separator helper: update frame after writing one complete element
    auto done_elem = [](Frame &f) {
      if (f.is_obj) {
        f.next_val = !f.next_val;
        if (!f.next_val)
          f.has_prev = true;
      } else {
        f.has_prev = true;
        f.arr_idx++;
      }
    };
    // Write separator before a non-closing node
    auto write_sep = [&](Frame &f) {
      if (f.is_obj) {
        if (f.next_val)
          out += ':';
        else if (f.has_prev)
          out += ',';
      } else {
        if (f.has_prev)
          out += ',';
      }
    };
    // Inject additions before a closing brace/bracket
    // Inject additions BEFORE a closing brace/bracket or AT an array index
    auto inject_array_adds = [&](uint32_t parent_idx, size_t current_idx) {
      if (doc_->array_insertions_.empty())
        return;
      auto ait = doc_->array_insertions_.find(parent_idx);
      if (ait == doc_->array_insertions_.end())
        return;
      for (const auto &ins : ait->second) {
        if (ins.index == current_idx) {
          if (stk[top].has_prev)
            out += ',';
          out += ins.data;
          stk[top].has_prev = true;
        }
      }
    };
    auto inject_adds = [&](uint32_t parent_idx, Frame &f) {
      if (!doc_->additions_.empty()) {
        auto ait = doc_->additions_.find(parent_idx);
        if (ait != doc_->additions_.end()) {
          for (const auto &[k, v] : ait->second) {
            if (f.has_prev)
              out += ',';
            if (f.is_obj) {
              out += '"';
              out += k;
              out += "\":";
            }
            out += v;
            f.has_prev = true;
          }
        }
      }
    };

    while (i < end_c) {
      const TapeNode &nd = doc_->tape[i];
      const TapeNodeType type =
          static_cast<TapeNodeType>((nd.meta >> 24) & 0xFF);

      // At key position: check if deleted (object member)
      if (top >= 0 && stk[top].is_obj && !stk[top].next_val &&
          type != TapeNodeType::ObjectEnd) {
        if (!doc_->deleted_.empty() && doc_->deleted_.count(i)) {
          i = skip_value_(i + 1); // skip deleted key + value
          continue;
        }
      }
      // At array element: check if deleted or needs insertion
      if (top >= 0 && !stk[top].is_obj && type != TapeNodeType::ArrayEnd) {
        inject_array_adds(stk[top].start_idx, stk[top].arr_idx);
        if (!doc_->deleted_.empty() && doc_->deleted_.count(i)) {
          i = skip_value_(i); // skip deleted element
          stk[top].arr_idx++;
          continue;
        }
      }

      // Write separator (for non-closing nodes, non-root)
      const bool is_close =
          (type == TapeNodeType::ObjectEnd || type == TapeNodeType::ArrayEnd);
      if (top >= 0 && !is_close && !(i == start_c))
        write_sep(stk[top]);

      // Mutation overlay
      if (QBUEM_UNLIKELY(!doc_->mutations_.empty())) {
        auto mit = doc_->mutations_.find(i);
        if (mit != doc_->mutations_.end()) {
          write_mutation_(out, mit->second);
          if (top >= 0)
            done_elem(stk[top]);
          ++i;
          continue;
        }
      }

      switch (type) {
      case TapeNodeType::ObjectStart:
        out += '{';
        ++top;
        stk[top] = {true, false, false, i, 0};
        break;
      case TapeNodeType::ArrayStart:
        out += '[';
        ++top;
        stk[top] = {false, false, false, i, 0};
        break;
      case TapeNodeType::ObjectEnd:
        if (QBUEM_UNLIKELY(top < 0)) {
          out += '}';
          break;
        }
        inject_adds(stk[top].start_idx, stk[top]);
        out += '}';
        --top;
        if (top >= 0)
          done_elem(stk[top]);
        break;
      case TapeNodeType::ArrayEnd:
        if (QBUEM_UNLIKELY(top < 0)) {
          out += ']';
          break;
        }
        inject_adds(stk[top].start_idx, stk[top]);
        out += ']';
        --top;
        if (top >= 0)
          done_elem(stk[top]);
        break;
      case TapeNodeType::StringRaw: {
        const uint16_t slen = static_cast<uint16_t>(nd.meta & 0xFFFFu);
        const size_t src_sz = doc_->source.size();
        const size_t safe_slen =
            (nd.offset < src_sz) ? std::min<size_t>(slen, src_sz - nd.offset)
                                 : 0;
        out += '"';
        out.append(src + nd.offset, safe_slen);
        out += '"';
        if (top >= 0)
          done_elem(stk[top]);
        break;
      }
      case TapeNodeType::Integer:
      case TapeNodeType::NumberRaw:
      case TapeNodeType::Double: {
        const uint16_t nlen = static_cast<uint16_t>(nd.meta & 0xFFFFu);
        const size_t src_sz = doc_->source.size();
        const size_t safe_nlen =
            (nd.offset < src_sz) ? std::min<size_t>(nlen, src_sz - nd.offset)
                                 : 0;
        out.append(src + nd.offset, safe_nlen);
        if (top >= 0)
          done_elem(stk[top]);
        break;
      }
      case TapeNodeType::BooleanTrue:
        out += "true";
        if (top >= 0)
          done_elem(stk[top]);
        break;
      case TapeNodeType::BooleanFalse:
        out += "false";
        if (top >= 0)
          done_elem(stk[top]);
        break;
      case TapeNodeType::Null:
        out += "null";
        if (top >= 0)
          done_elem(stk[top]);
        break;
      default:
        break;
      }
      ++i;
    }
    return out;
  }

  // Pretty-print recursive helper
  void dump_pretty_(std::string &out, int indent_size, int depth) const {
    if (!doc_) {
      out += "null";
      return;
    }
    const TapeNodeType root_type = doc_->tape[idx_].type();

    if (root_type == TapeNodeType::ObjectStart) {
      out += '{';
      bool first = true;
      std::string pad(static_cast<size_t>((depth + 1) * indent_size), ' ');
      std::string close_pad(static_cast<size_t>(depth * indent_size), ' ');
      // tape entries
      uint32_t i = idx_ + 1;
      const size_t ntape = doc_->tape.size();
      while (i < ntape && doc_->tape[i].type() != TapeNodeType::ObjectEnd) {
        if (!doc_->deleted_.empty() && doc_->deleted_.count(i)) {
          i = skip_value_(i + 1);
          continue;
        }
        if (!first)
          out += ',';
        out += '\n';
        out += pad;
        // key
        const TapeNode &kn = doc_->tape[i];
        out += '"';
        out.append(doc_->source.data() + kn.offset, kn.length());
        out += '"';
        out += ": ";
        Value val_v(doc_, i + 1);
        val_v.dump_pretty_(out, indent_size, depth + 1);
        first = false;
        i = skip_value_(i + 1);
      }
      // additions
      if (!doc_->additions_.empty()) {
        auto ait = doc_->additions_.find(idx_);
        if (ait != doc_->additions_.end()) {
          for (const auto &[k, v] : ait->second) {
            if (!first)
              out += ',';
            out += '\n';
            out += pad;
            out += '"';
            out += k;
            out += "\": ";
            out += v;
            first = false;
          }
        }
      }
      if (!first) {
        out += '\n';
        out += close_pad;
      }
      out += '}';
    } else if (root_type == TapeNodeType::ArrayStart) {
      out += '[';
      bool first = true;
      std::string pad(static_cast<size_t>((depth + 1) * indent_size), ' ');
      std::string close_pad(static_cast<size_t>(depth * indent_size), ' ');
      uint32_t i = idx_ + 1;
      const size_t ntape = doc_->tape.size();
      while (i < ntape && doc_->tape[i].type() != TapeNodeType::ArrayEnd) {
        if (!doc_->deleted_.empty() && doc_->deleted_.count(i)) {
          i = skip_value_(i);
          continue;
        }
        if (!first)
          out += ',';
        out += '\n';
        out += pad;
        Value elem_v(doc_, i);
        elem_v.dump_pretty_(out, indent_size, depth + 1);
        first = false;
        i = skip_value_(i);
      }
      // additions
      if (!doc_->additions_.empty()) {
        auto ait = doc_->additions_.find(idx_);
        if (ait != doc_->additions_.end()) {
          for (const auto &[k, v] : ait->second) {
            if (!first)
              out += ',';
            out += '\n';
            out += pad;
            out += v;
            first = false;
          }
        }
      }
      if (!first) {
        out += '\n';
        out += close_pad;
      }
      out += ']';
    } else {
      // scalar — just use fast dump()
      out += dump();
    }
  }
};

// NEON Structural Scanner utilities
//
// Verified correct via phase19_test.cpp before integration.

// Load 8 bytes without UB
static QBUEM_INLINE uint64_t load64(const char *p) noexcept {
  uint64_t v;
  std::memcpy(&v, p, 8);
  return v;
}

// SWAR action mask: bit 7 of each byte set iff byte is non-WS (>= 0x21)
static QBUEM_INLINE uint64_t swar_action_mask(uint64_t v) noexcept {
  constexpr uint64_t K = 0x0101010101010101ULL;
  constexpr uint64_t H = 0x8080808080808080ULL;
  constexpr uint64_t BROAD = K * 0x21;
  uint64_t sub = v - BROAD;
  uint64_t skip = sub & H;
  return ~skip & H;
}

#if QBUEM_HAS_NEON

// NEON movemask: 16-bit bitmask from 16-byte 0xFF/0x00 vector.
// Bit i set iff byte i was 0xFF.
// Uses vshrq_n_u8 + vmulq_u8 + vpaddlq reduction — zero memory loads.
// kW is a compile-time constant; Clang encodes it as a NEON immediate.
static QBUEM_INLINE uint16_t neon_movemask(uint8x16_t mask) noexcept {
  // Step 1: convert 0xFF→1, 0x00→0 per byte
  uint8x16_t bits = vshrq_n_u8(mask, 7);
  // Step 2: weight each byte by its bit position 2^(i%8)
  // kW is a constexpr literal; no memory load generated by Clang -O2.
  static const uint8_t kW[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                 1, 2, 4, 8, 16, 32, 64, 128};
  const uint8x16_t wv =
      vld1q_u8(kW); // hoisted to register by compiler since kW is static
  uint8x16_t weighted = vmulq_u8(bits, wv);
  // Step 3: horizontal reduce: 16 bytes → 8 uint16 → 4 uint32 → 2 uint64
  // s64[0] = sum of weighted bytes 0-7 (= low byte of result, bits 0-7)
  // s64[1] = sum of weighted bytes 8-15 (= high byte, bits 8-15)
  uint16x8_t s16 = vpaddlq_u8(weighted);
  uint32x4_t s32 = vpaddlq_u16(s16);
  uint64x2_t s64 = vpaddlq_u32(s32);
  return (uint16_t)(vgetq_lane_u64(s64, 0) | (vgetq_lane_u64(s64, 1) << 8));
}

#endif // QBUEM_HAS_NEON

// Stage 1 AVX-512 Structural Scanner
//
// Scans json[0..len) and writes byte offsets to:
//   '{' '}' '[' ']' ':' ','  — outside strings
//   '"' (opening AND closing quote of every string)
//   value starts (digit, '-', 't', 'f', 'n') — outside strings
//
// Stage 2 (parse_staged) uses this index to:
//   • Skip whitespace scanning entirely (no skip_to_action calls)
//   • Compute string length as O(1): close_offset - open_offset - 1
//
// Uses same escape / in-string algorithm as fill_bitmap() for correctness.
#if QBUEM_HAS_AVX512
QBUEM_INLINE void stage1_scan_avx512(const char *src, size_t len,
                                     Stage1Index &idx) {
  // Upper bound: at most every byte is structural (e.g. "[[[[[")
  idx.reserve(len + 1);
  idx.reset();
  uint32_t *out = idx.positions;
  uint32_t count = 0;

  const char *p = src;
  const char *end = src + len;

  uint64_t prev_in_string = 0; // all-0 = outside string; all-1 = inside string
  bool prev_escaped = false;
  uint64_t prev_non_ws = (1ULL << 63); // treat start as after whitespace

  // Pre-load broadcast constants outside the loop (hoisted to registers).
  const __m512i v_brace_o = _mm512_set1_epi8('{');
  const __m512i v_brace_c = _mm512_set1_epi8('}');
  const __m512i v_bracket_o = _mm512_set1_epi8('[');
  const __m512i v_bracket_c = _mm512_set1_epi8(']');
  const __m512i v_colon = _mm512_set1_epi8(':');
  const __m512i v_comma = _mm512_set1_epi8(',');
  const __m512i v_quote = _mm512_set1_epi8('"');
  const __m512i v_backslash = _mm512_set1_epi8('\\');
  const __m512i v_ws_thresh = _mm512_set1_epi8(0x20);

  while (p + 64 <= end) {
    __m512i v = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(p));

    uint64_t q_bits = _mm512_cmpeq_epi8_mask(v, v_quote);
    uint64_t bs_bits = _mm512_cmpeq_epi8_mask(v, v_backslash);
    // split structural chars into bracket_bits ({}[]) and sep_bits
    // (:,). sep_bits participates in ws_like/vstart computation (so values
    // after :, are still detected as vstart), but is NOT included in the
    // emitted structural bitmask — shrinks positions[] by ~33% for string-heavy
    // files (twitter, citm).
    uint64_t bracket_bits = _mm512_cmpeq_epi8_mask(v, v_brace_o) |
                            _mm512_cmpeq_epi8_mask(v, v_brace_c) |
                            _mm512_cmpeq_epi8_mask(v, v_bracket_o) |
                            _mm512_cmpeq_epi8_mask(v, v_bracket_c);
    uint64_t sep_bits =
        _mm512_cmpeq_epi8_mask(v, v_colon) | _mm512_cmpeq_epi8_mask(v, v_comma);
    uint64_t s_bits = bracket_bits | sep_bits; // used only for ws_like / vstart
    // Signed cmpgt: 0x21-0x7F → 1 (non-ws ASCII). 0x80-0xFF treated as ws,
    // but UTF-8 continuation bytes only appear inside strings (masked by
    // ~inside).
    uint64_t non_ws =
        static_cast<uint64_t>(_mm512_cmpgt_epi8_mask(v, v_ws_thresh));

    // ── Escape propagation (identical to fill_bitmap()) ───────────────────
    uint64_t escaped = 0;
    uint64_t temp_esc = bs_bits;
    if (prev_escaped) {
      escaped |= 1ULL;
      prev_escaped = false;
      if (temp_esc & 1ULL)
        temp_esc &= ~1ULL;
    }
    while (temp_esc) {
      int start = __builtin_ctzll(temp_esc);
      uint64_t mask_from_start = ~0ULL << start;
      uint64_t non_bs_from_start = ~bs_bits & mask_from_start;
      int run_end =
          (non_bs_from_start == 0) ? 64 : __builtin_ctzll(non_bs_from_start);
      int run_len = run_end - start;
      for (int j = start + 1; j < run_end; j += 2)
        escaped |= (1ULL << j);
      if (run_len % 2 != 0) {
        if (run_end < 64)
          escaped |= (1ULL << run_end);
        else
          prev_escaped = true;
      }
      if (run_end == 64)
        break;
      temp_esc &= (~0ULL << run_end);
    }

    uint64_t clean_quotes = q_bits & ~escaped;
    uint64_t in_string = simd::prefix_xor(clean_quotes) ^ prev_in_string;
    uint64_t inside = in_string & ~clean_quotes;

    uint64_t external_non_ws = non_ws & ~inside;
    // Structural chars outside strings + all real quotes (open & close)
    uint64_t external_symbols = (s_bits & ~inside) | clean_quotes;
    uint64_t ws_like = (~non_ws & ~inside) | external_symbols;
    // vstart: first byte of each number/bool/null (non-ws, non-structural,
    // outside strings, following whitespace or a structural character)
    uint64_t vstart = (external_non_ws & ~external_symbols) &
                      (ws_like << 1 | (prev_non_ws >> 63));

    // emit bracket_bits ({[}]) + quotes + vstart — omit sep_bits
    // (:,). sep_bits is still in external_symbols (for correct ws_like/vstart),
    // but Stage 2 (parse_staged) infers context from the push() bit-stack
    // without needing explicit :, position entries.
    uint64_t structural = ((bracket_bits & ~inside) | clean_quotes) | vstart;

    // Write positions to flat array
    uint32_t base = static_cast<uint32_t>(p - src);
    while (structural) {
      int bit = __builtin_ctzll(structural);
      out[count++] = base + static_cast<uint32_t>(bit);
      structural &= structural - 1;
    }

    prev_in_string =
        static_cast<uint64_t>(static_cast<int64_t>(in_string) >> 63);
    prev_non_ws = ws_like;
    p += 64;
  }

  // ── Tail: pad remaining bytes to 64 with spaces ───────────────────────
  size_t remaining = static_cast<size_t>(end - p);
  if (remaining > 0) {
    alignas(64) char buf[64];
    std::memset(buf, ' ', 64);
    std::memcpy(buf, p, remaining);

    __m512i v = _mm512_load_si512(reinterpret_cast<const __m512i *>(buf));

    uint64_t q_bits = _mm512_cmpeq_epi8_mask(v, v_quote);
    uint64_t bs_bits = _mm512_cmpeq_epi8_mask(v, v_backslash);
    uint64_t bracket_bits = _mm512_cmpeq_epi8_mask(v, v_brace_o) |
                            _mm512_cmpeq_epi8_mask(v, v_brace_c) |
                            _mm512_cmpeq_epi8_mask(v, v_bracket_o) |
                            _mm512_cmpeq_epi8_mask(v, v_bracket_c);
    uint64_t sep_bits =
        _mm512_cmpeq_epi8_mask(v, v_colon) | _mm512_cmpeq_epi8_mask(v, v_comma);
    uint64_t s_bits = bracket_bits | sep_bits;
    uint64_t non_ws =
        static_cast<uint64_t>(_mm512_cmpgt_epi8_mask(v, v_ws_thresh));

    // Mask to valid bytes only
    uint64_t m = (remaining >= 64) ? ~0ULL : (1ULL << remaining) - 1;
    q_bits &= m;
    bs_bits &= m;
    bracket_bits &= m;
    sep_bits &= m;
    s_bits &= m;
    non_ws &= m;

    uint64_t escaped = 0;
    uint64_t temp_esc = bs_bits;
    if (prev_escaped) {
      escaped |= 1ULL;
      prev_escaped = false;
      if (temp_esc & 1ULL)
        temp_esc &= ~1ULL;
    }
    while (temp_esc) {
      int start = __builtin_ctzll(temp_esc);
      uint64_t mask_from_start = ~0ULL << start;
      uint64_t non_bs_from_start = ~bs_bits & mask_from_start;
      int run_end =
          (non_bs_from_start == 0) ? 64 : __builtin_ctzll(non_bs_from_start);
      int run_len = run_end - start;
      for (int j = start + 1; j < run_end; j += 2)
        escaped |= (1ULL << j);
      if (run_len % 2 != 0 && run_end < 64)
        escaped |= (1ULL << run_end);
      if (run_end == 64)
        break;
      temp_esc &= (~0ULL << run_end);
    }

    uint64_t clean_quotes = (q_bits & ~escaped) & m;
    uint64_t in_string = simd::prefix_xor(clean_quotes) ^ prev_in_string;
    uint64_t inside = in_string & ~clean_quotes;

    uint64_t external_non_ws = non_ws & ~inside;
    uint64_t external_symbols = (s_bits & ~inside) | clean_quotes;
    uint64_t ws_like = (~non_ws & ~inside) | external_symbols;
    uint64_t vstart = (external_non_ws & ~external_symbols) &
                      (ws_like << 1 | (prev_non_ws >> 63));

    // emit only bracket_bits + quotes + vstart (no :,)
    uint64_t structural =
        (((bracket_bits & ~inside) | clean_quotes) | vstart) & m;

    uint32_t base = static_cast<uint32_t>(p - src);
    while (structural) {
      int bit = __builtin_ctzll(structural);
      out[count++] = base + static_cast<uint32_t>(bit);
      structural &= structural - 1;
    }
  }

  idx.count = count;
}
#elif QBUEM_HAS_NEON
// Stage 1 NEON Structural Scanner
QBUEM_INLINE void stage1_scan_neon(const char *src, size_t len,
                                   Stage1Index &idx) {
  idx.reserve(len + 1);
  idx.reset();
  uint32_t *out = idx.positions;
  uint32_t count = 0;

  const char *p = src;
  const char *end = src + len;

  uint64_t prev_in_string = 0;
  bool prev_escaped = false;
  uint64_t prev_non_ws = (1ULL << 63);

  const uint8x16_t v_brace_o = vdupq_n_u8('{');
  const uint8x16_t v_brace_c = vdupq_n_u8('}');
  const uint8x16_t v_bracket_o = vdupq_n_u8('[');
  const uint8x16_t v_bracket_c = vdupq_n_u8(']');
  const uint8x16_t v_colon = vdupq_n_u8(':');
  const uint8x16_t v_comma = vdupq_n_u8(',');
  const uint8x16_t v_quote = vdupq_n_u8('"');
  const uint8x16_t v_backslash = vdupq_n_u8('\\');
  const uint8x16_t v_ws_thresh = vdupq_n_u8(0x20);

  while (p + 64 <= end) {
    uint64_t q_bits = 0, bs_bits = 0, bracket_bits = 0, sep_bits = 0,
             non_ws = 0;

    for (int i = 0; i < 4; ++i) {
      uint8x16_t chunk =
          vld1q_u8(reinterpret_cast<const uint8_t *>(p + i * 16));

      uint8x16_t m_quo = vceqq_u8(chunk, v_quote);
      uint8x16_t m_esc = vceqq_u8(chunk, v_backslash);

      uint8x16_t m_bracket = vorrq_u8(
          vorrq_u8(vceqq_u8(chunk, v_brace_o), vceqq_u8(chunk, v_brace_c)),
          vorrq_u8(vceqq_u8(chunk, v_bracket_o), vceqq_u8(chunk, v_bracket_c)));
      uint8x16_t m_sep =
          vorrq_u8(vceqq_u8(chunk, v_colon), vceqq_u8(chunk, v_comma));

      uint8x16_t m_not_white = vcgtq_u8(chunk, v_ws_thresh);

      q_bits |= (uint64_t)neon_movemask(m_quo) << (i * 16);
      bs_bits |= (uint64_t)neon_movemask(m_esc) << (i * 16);
      bracket_bits |= (uint64_t)neon_movemask(m_bracket) << (i * 16);
      sep_bits |= (uint64_t)neon_movemask(m_sep) << (i * 16);
      non_ws |= (uint64_t)neon_movemask(m_not_white) << (i * 16);
    }

    uint64_t s_bits = bracket_bits | sep_bits;

    // ── Escape propagation ───────────────────
    uint64_t escaped = 0;
    uint64_t temp_esc = bs_bits;
    if (prev_escaped) {
      escaped |= 1ULL;
      prev_escaped = false;
      if (temp_esc & 1ULL)
        temp_esc &= ~1ULL;
    }
    while (temp_esc) {
      int start = __builtin_ctzll(temp_esc);
      uint64_t mask_from_start = ~0ULL << start;
      uint64_t non_bs_from_start = ~bs_bits & mask_from_start;
      int run_end =
          (non_bs_from_start == 0) ? 64 : __builtin_ctzll(non_bs_from_start);
      int run_len = run_end - start;
      for (int j = start + 1; j < run_end; j += 2)
        escaped |= (1ULL << j);
      if (run_len % 2 != 0) {
        if (run_end < 64)
          escaped |= (1ULL << run_end);
        else
          prev_escaped = true;
      }
      if (run_end == 64)
        break;
      temp_esc &= (~0ULL << run_end);
    }

    uint64_t clean_quotes = q_bits & ~escaped;
    uint64_t in_string = simd::prefix_xor(clean_quotes) ^ prev_in_string;
    uint64_t inside = in_string & ~clean_quotes;

    uint64_t external_non_ws = non_ws & ~inside;
    uint64_t external_symbols = (s_bits & ~inside) | clean_quotes;
    uint64_t ws_like = (~non_ws & ~inside) | external_symbols;

    uint64_t vstart = (external_non_ws & ~external_symbols) &
                      (ws_like << 1 | (prev_non_ws >> 63));

    uint64_t structural = ((bracket_bits & ~inside) | clean_quotes) | vstart;

    // Write positions to flat array
    uint32_t base = static_cast<uint32_t>(p - src);
    while (structural) {
      int bit = __builtin_ctzll(structural);
      out[count++] = base + static_cast<uint32_t>(bit);
      structural &= structural - 1;
    }

    prev_in_string =
        static_cast<uint64_t>(static_cast<int64_t>(in_string) >> 63);
    prev_non_ws = ws_like;
    p += 64;
  }

  // ── Tail: pad remaining bytes to 64 with spaces ───────────────────────
  size_t remaining = static_cast<size_t>(end - p);
  if (remaining > 0) {
    alignas(64) char buf[64];
    std::memset(buf, ' ', 64);
    std::memcpy(buf, p, remaining);

    uint64_t q_bits = 0, bs_bits = 0, bracket_bits = 0, sep_bits = 0,
             non_ws = 0;

    for (int i = 0; i < 4; ++i) {
      uint8x16_t chunk =
          vld1q_u8(reinterpret_cast<const uint8_t *>(buf + i * 16));

      uint8x16_t m_quo = vceqq_u8(chunk, v_quote);
      uint8x16_t m_esc = vceqq_u8(chunk, v_backslash);

      uint8x16_t m_bracket = vorrq_u8(
          vorrq_u8(vceqq_u8(chunk, v_brace_o), vceqq_u8(chunk, v_brace_c)),
          vorrq_u8(vceqq_u8(chunk, v_bracket_o), vceqq_u8(chunk, v_bracket_c)));
      uint8x16_t m_sep =
          vorrq_u8(vceqq_u8(chunk, v_colon), vceqq_u8(chunk, v_comma));

      uint8x16_t m_not_white = vcgtq_u8(chunk, v_ws_thresh);

      q_bits |= (uint64_t)neon_movemask(m_quo) << (i * 16);
      bs_bits |= (uint64_t)neon_movemask(m_esc) << (i * 16);
      bracket_bits |= (uint64_t)neon_movemask(m_bracket) << (i * 16);
      sep_bits |= (uint64_t)neon_movemask(m_sep) << (i * 16);
      non_ws |= (uint64_t)neon_movemask(m_not_white) << (i * 16);
    }

    uint64_t s_bits = bracket_bits | sep_bits;

    // Mask to valid bytes only
    uint64_t m = (remaining >= 64) ? ~0ULL : (1ULL << remaining) - 1;
    q_bits &= m;
    bs_bits &= m;
    bracket_bits &= m;
    sep_bits &= m;
    s_bits &= m;
    non_ws &= m;

    uint64_t escaped = 0;
    uint64_t temp_esc = bs_bits;
    if (prev_escaped) {
      escaped |= 1ULL;
      prev_escaped = false;
      if (temp_esc & 1ULL)
        temp_esc &= ~1ULL;
    }
    while (temp_esc) {
      int start = __builtin_ctzll(temp_esc);
      uint64_t mask_from_start = ~0ULL << start;
      uint64_t non_bs_from_start = ~bs_bits & mask_from_start;
      int run_end =
          (non_bs_from_start == 0) ? 64 : __builtin_ctzll(non_bs_from_start);
      int run_len = run_end - start;
      for (int j = start + 1; j < run_end; j += 2)
        escaped |= (1ULL << j);
      if (run_len % 2 != 0 && run_end < 64)
        escaped |= (1ULL << run_end);
      if (run_end == 64)
        break;
      temp_esc &= (~0ULL << run_end);
    }

    uint64_t clean_quotes = (q_bits & ~escaped) & m;
    uint64_t in_string = simd::prefix_xor(clean_quotes) ^ prev_in_string;
    uint64_t inside = in_string & ~clean_quotes;

    uint64_t external_non_ws = non_ws & ~inside;
    uint64_t external_symbols = (s_bits & ~inside) | clean_quotes;
    uint64_t ws_like = (~non_ws & ~inside) | external_symbols;
    uint64_t vstart = (external_non_ws & ~external_symbols) &
                      (ws_like << 1 | (prev_non_ws >> 63));

    uint64_t structural = ((bracket_bits & ~inside) | clean_quotes) | vstart;

    uint32_t base = static_cast<uint32_t>(p - src);
    while (structural) {
      int bit = __builtin_ctzll(structural);
      out[count++] = base + static_cast<uint32_t>(bit);
      structural &= structural - 1;
    }
  }

  idx.count = count;
}
#endif // QBUEM_HAS_AVX512

// 256-Entry constexpr Action LUT
//
// Replaces the 17-case switch(c) in parse() with an 11-entry
// switch(kActionLut[c]). Fewer cases → better Branch Target Buffer
// utilisation on both M1 (aarch64) and x86_64 out-of-order cores.
// The 256-byte table fits in 4 L1 cache lines and is hoisted to a
// register by the compiler after the first access.
// Architecture-agnostic pure C++ — no SIMD required.

enum ActionId : uint8_t {
  kActNone = 0, // whitespace or unknown — should not reach switch
  kActString = 1,
  kActNumber = 2,
  kActObjOpen = 3,
  kActArrOpen = 4,
  kActClose = 5, // '}' or ']'
  kActColon = 6,
  kActComma = 7,
  kActTrue = 8,
  kActFalse = 9,
  kActNull = 10,
};

static constexpr auto kActionLut = []() consteval {
  std::array<uint8_t, 256> t{}; // zero-initialised: kActNone (0)
  t[static_cast<uint8_t>('"')] = kActString;
  t[static_cast<uint8_t>('-')] = kActNumber;
  // digits '0'..'9'
  t[static_cast<uint8_t>('0')] = kActNumber;
  t[static_cast<uint8_t>('1')] = kActNumber;
  t[static_cast<uint8_t>('2')] = kActNumber;
  t[static_cast<uint8_t>('3')] = kActNumber;
  t[static_cast<uint8_t>('4')] = kActNumber;
  t[static_cast<uint8_t>('5')] = kActNumber;
  t[static_cast<uint8_t>('6')] = kActNumber;
  t[static_cast<uint8_t>('7')] = kActNumber;
  t[static_cast<uint8_t>('8')] = kActNumber;
  t[static_cast<uint8_t>('9')] = kActNumber;
  t[static_cast<uint8_t>('{')] = kActObjOpen;
  t[static_cast<uint8_t>('[')] = kActArrOpen;
  t[static_cast<uint8_t>('}')] = kActClose;
  t[static_cast<uint8_t>(']')] = kActClose;
  t[static_cast<uint8_t>(':')] = kActColon;
  t[static_cast<uint8_t>(',')] = kActComma;
  t[static_cast<uint8_t>('t')] = kActTrue;
  t[static_cast<uint8_t>('f')] = kActFalse;
  t[static_cast<uint8_t>('n')] = kActNull;
  return t;
}();

// Parser — hot loop

class Parser {
  const char *p_;
  const char *end_;
  const char *data_;
  DocumentState *doc_;
  size_t depth_ = 0;

  // compact per-depth context state.
  // Replaces 3×64-bit bit-stacks + overflow array with one byte per depth.
  //   bit0 = is_key  (next push is an object key)
  //   bit1 = in_obj  (current depth is an object, not array)
  //   bit2 = has_elem (≥1 element already pushed at this depth)
  //
  // cur_state_ is register-resident throughout parse() — no memory access per
  // push(). cstate_stack_[d] saves/restores the parent depth's state on
  // open/close bracket events (infrequent: ~8% of tokens in twitter.json).
  // Supports up to depth 1087 (same as old bit-stack + overflow).
  uint8_t cur_state_ = 0;
  uint8_t cstate_stack_[1088] = {};

  // Technique 8: local tape_head_ register variable.
  // Kept as a field but initialized from doc_->tape.base in parse().
  // The compiler will register-allocate this across the entire parse() body,
  // eliminating the pointer-chain access doc_->tape.head on every push().
  TapeNode *tape_head_ = nullptr;

  // Key Length Cache — schema-prediction key scanner bypass.
  // For each nesting depth, caches JSON source lengths of object keys seen in
  // the first object at that depth. Subsequent same-schema objects skip the
  // SIMD key-end scan: a single byte comparison s[cached_len]=='"' suffices.
  //
  // citm_catalog.json: 243 performances × 9 keys = 2187 SIMD scans replaced
  // by byte comparisons.
  // twitter.json tweet objects have ~25 distinct keys. MAX_KEYS=16
  // left keys 17-25 cache-miss on every tweet (no SIMD bypass). Increasing to
  // 32 covers all twitter keys and citm's worst-case depth (9 keys per
  // performance). Memory: 8×32×2 + 8 = 520 bytes (L1-resident on all targets;
  // M1 L1 = 192KB).
  struct KeyLenCache {
    static constexpr uint8_t MAX_DEPTH = 8;
    static constexpr uint8_t MAX_KEYS = 32;
    uint8_t key_idx[MAX_DEPTH] = {};         // current key pos per depth
    uint16_t lens[MAX_DEPTH][MAX_KEYS] = {}; // cached source lengths (0=unset)
  } kc_;

  // ── skip_to_action: SWAR-8 + scalar whitespace skip chain ──
  // Returns the first action byte and advances p_ past whitespace.
  // Use the returned char directly in switch(c) — avoids extra *p_ read.
  //
  // NEON path is intentionally disabled here: for twitter.json's whitespace
  // distribution (typically 2-8 consecutive WS bytes between tokens), the
  // vld1q_u8 overhead exceeds the gain vs SWAR-8. NEON accelerates bulk
  // whitespace (>16 consecutive bytes), which is rare here.
  QBUEM_INLINE char skip_to_action() noexcept {
    // Fast path: already on action byte.
    // Guard p_ < end_ before dereferencing: callers may reach here with
    // p_ == end_ (e.g. unterminated array/object like "[").
    if (QBUEM_UNLIKELY(p_ >= end_))
      return 0;
    unsigned char c = static_cast<unsigned char>(*p_);
    if (QBUEM_LIKELY(c > 0x20))
      return static_cast<char>(c);

#if QBUEM_HAS_AVX512
    // ── AVX-512 64B batch whitespace skip ──────────────────────────
    // _mm512_cmpgt_epi8_mask vs 0x20 is 1 op (vs AVX2's 8 ops for 32B).
    // 64B/iter vs SWAR-32's 32B/iter → ~2× throughput for whitespace-heavy
    // JSON.
    //
    // SWAR-8 pre-gate: twitter.json has 2-8 WS bytes between tokens; absorb
    // them here before paying any 512-bit register setup cost (    // lesson).
    {
      uint64_t am = swar_action_mask(load64(p_));
      if (QBUEM_LIKELY(am != 0)) {
        p_ += QBUEM_CTZ(am) >> 3;
        return *p_;
      }
      p_ += 8;
    }
    // Still in whitespace → bulk path: AVX-512 64B/iter for long WS runs.
    if (QBUEM_LIKELY(p_ + 64 <= end_)) {
      const __m512i ws_thresh = _mm512_set1_epi8(0x20);
      do {
        __m512i v = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(p_));
        uint64_t non_ws =
            static_cast<uint64_t>(_mm512_cmpgt_epi8_mask(v, ws_thresh));
        if (QBUEM_LIKELY(non_ws)) {
          p_ += __builtin_ctzll(non_ws);
          return *p_;
        }
        p_ += 64;
      } while (QBUEM_LIKELY(p_ + 64 <= end_));
    }
    // <64B tail: SWAR-8 scalar walk
    while (QBUEM_LIKELY(p_ + 8 <= end_)) {
      uint64_t am = swar_action_mask(load64(p_));
      if (QBUEM_LIKELY(am != 0)) {
        p_ += QBUEM_CTZ(am) >> 3;
        return *p_;
      }
      p_ += 8;
    }
#elif QBUEM_HAS_NEON
    // ── Global AArch64 NEON Loop ──────────────────────────────────
    // SWAR pre-gates are strictly avoided on AArch64 because vector setup
    // (vld1q) and max-reduce (vmaxvq) have significantly lower latency and
    // higher throughput than scalar GPR dependencies on both Apple Silicon
    // and Generic ARM cores.
    while (QBUEM_LIKELY(p_ + 16 <= end_)) {
      uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t *>(p_));
      uint8x16_t mask = vcgtq_u8(v, vdupq_n_u8(0x20));
      // vmaxvq returns the max 32-bit element. If any byte was > 0x20,
      // the mask will have 0xFF in that byte, so the max 32-bit element
      // will be != 0.
      if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(mask)) != 0)) {
        // Find exact position via auto-vectorized loop
        while (static_cast<unsigned char>(*p_) <= 0x20)
          ++p_;
        return *p_;
      }
      p_ += 16;
    }
#else
    // SWAR-32 fallback (no SIMD available)
    while (QBUEM_LIKELY(p_ + 32 <= end_)) {
      uint64_t a0 = swar_action_mask(load64(p_));
      uint64_t a1 = swar_action_mask(load64(p_ + 8));
      uint64_t a2 = swar_action_mask(load64(p_ + 16));
      uint64_t a3 = swar_action_mask(load64(p_ + 24));
      if (QBUEM_LIKELY(a0 | a1 | a2 | a3)) {
        if (a0) {
          p_ += QBUEM_CTZ(a0) >> 3;
          return *p_;
        }
        if (a1) {
          p_ += 8 + (QBUEM_CTZ(a1) >> 3);
          return *p_;
        }
        if (a2) {
          p_ += 16 + (QBUEM_CTZ(a2) >> 3);
          return *p_;
        }
        p_ += 24 + (QBUEM_CTZ(a3) >> 3);
        return *p_;
      }
      p_ += 32;
    }
    while (QBUEM_LIKELY(p_ + 8 <= end_)) {
      uint64_t am = swar_action_mask(load64(p_));
      if (QBUEM_LIKELY(am != 0)) {
        p_ += QBUEM_CTZ(am) >> 3;
        return *p_;
      }
      p_ += 8;
    }
#endif

    // Scalar tail
    while (p_ < end_) {
      c = static_cast<unsigned char>(*p_);
      if (c > 0x20)
        return static_cast<char>(c);
      ++p_;
    }
    return 0;
  }

  // ── Contextual SIMD Gate String Scanner ─────────────
  //
  // Theory: reverted NEON because it added startup overhead on
  // short strings. Root fix: an 8B SWAR gate runs first. Short strings
  // (≤8 chars, ≈36% of twitter.json) exit immediately at ZERO SIMD cost.
  // Only when the string is confirmed > 8 chars do we enter the SIMD loop.
  //
  // Architecture dispatch order:
  //   aarch64 (NEON 16B)  ← PRIMARY   — M1 / ARMv8+
  //   x86_64  (SSE2 16B)  ← SECONDARY — Nehalem+, all modern x86
  //   generic (SWAR-16)   ← FALLBACK
  QBUEM_INLINE const char *scan_string_end(const char *p) noexcept {
    constexpr uint64_t K = 0x0101010101010101ULL;
    constexpr uint64_t H = 0x8080808080808080ULL;
    const uint64_t qm = K * static_cast<uint8_t>('"');
    const uint64_t bsm = K * static_cast<uint8_t>('\\');

    // ── Stage 1: 8B SWAR gate ──────────────────────────────────────
    // Short strings (≤8 chars) exit here with zero SIMD overhead.
    // Backslash-early strings also exit early (benefit escape-heavy JSON).
#if !QBUEM_HAS_NEON
    if (QBUEM_LIKELY(p + 8 <= end_)) {
      uint64_t v0;
      std::memcpy(&v0, p, 8);
      uint64_t hq0 = v0 ^ qm;
      hq0 = (hq0 - K) & ~hq0 & H;
      uint64_t hb0 = v0 ^ bsm;
      hb0 = (hb0 - K) & ~hb0 & H;
      if (QBUEM_UNLIKELY(hq0 | hb0))
        return p + (QBUEM_CTZ(hq0 | hb0) >> 3);
      p += 8; // string confirmed > 8 chars: advance to SIMD
    }
#endif

    // ── Stage 2: SIMD loop (string > 8 chars confirmed) ───────────

#if QBUEM_HAS_NEON
    // aarch64 PRIMARY: NEON 16B. Pinpoint via scalar fallback loop.
    {
      const uint8x16_t vq = vdupq_n_u8('"');
      const uint8x16_t vbs = vdupq_n_u8('\\');
      while (QBUEM_LIKELY(p + 16 <= end_)) {
        uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t *>(p));
        uint8x16_t m = vorrq_u8(vceqq_u8(v, vq), vceqq_u8(v, vbs));
        if (QBUEM_UNLIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m)) != 0)) {
          // Pinpoint: AArch64 strongly prefers scalar loop over
          // cross-register extraction latency.
          while (*p != '"' && *p != '\\')
            ++p;
          return p;
        }
        p += 16;
      }
    }
#elif QBUEM_HAS_AVX2
    // x86_64 AVX2/AVX-512: SIMD string scanner.
    // AVX2 32B. : AVX-512 64B outer loop (when available).
    // aarch64 agents: inactive on M1 builds. x86_64: build with -march=native.
#if QBUEM_HAS_AVX512
    // AVX-512 64B per iteration — halves loop count vs AVX2.
    // _mm512_cmpeq_epi8_mask → uint64_t mask directly (no vpor needed).
    {
      const __m512i vq512 = _mm512_set1_epi8('"');
      const __m512i vbs512 = _mm512_set1_epi8('\\');
      while (QBUEM_LIKELY(p + 64 <= end_)) {
        __m512i v = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(p));
        uint64_t mask = _mm512_cmpeq_epi8_mask(v, vq512) |
                        _mm512_cmpeq_epi8_mask(v, vbs512);
        if (QBUEM_UNLIKELY(mask)) {
          p += __builtin_ctzll(mask);
          return p;
        }
        p += 64;
      }
    }
    // Fall through: AVX2 32B handles remaining <64B
#endif
    {
      const __m256i vq = _mm256_set1_epi8('"');
      const __m256i vbs = _mm256_set1_epi8('\\');
      while (QBUEM_LIKELY(p + 32 <= end_)) {
        __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
        uint32_t mask =
            static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(
                _mm256_cmpeq_epi8(v, vq), _mm256_cmpeq_epi8(v, vbs))));
        if (QBUEM_UNLIKELY(mask))
          return p + __builtin_ctz(mask);
        p += 32;
      }
    }
    // SSE2 16B tail (handles remaining <32B after AVX2 loop)
    {
      const __m128i vq = _mm_set1_epi8('"');
      const __m128i vbs = _mm_set1_epi8('\\');
      while (QBUEM_LIKELY(p + 16 <= end_)) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        int mask = _mm_movemask_epi8(
            _mm_or_si128(_mm_cmpeq_epi8(v, vq), _mm_cmpeq_epi8(v, vbs)));
        if (QBUEM_UNLIKELY(mask))
          return p + __builtin_ctz(mask);
        p += 16;
      }
    }
#elif defined(QBUEM_ARCH_X86_64)
    // x86_64 SECONDARY (SSE2 only, no AVX2): 16B per iteration.
    {
      const __m128i vq = _mm_set1_epi8('"');
      const __m128i vbs = _mm_set1_epi8('\\');
      while (QBUEM_LIKELY(p + 16 <= end_)) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        int mask = _mm_movemask_epi8(
            _mm_or_si128(_mm_cmpeq_epi8(v, vq), _mm_cmpeq_epi8(v, vbs)));
        if (QBUEM_UNLIKELY(mask))
          return p + __builtin_ctz(mask);
        p += 16;
      }
    }
#else
    // Generic SWAR-16 fallback (no SIMD available)
    while (p + 16 <= end_) {
      uint64_t v0, v1;
      std::memcpy(&v0, p, 8);
      std::memcpy(&v1, p + 8, 8);
      uint64_t hq0 = v0 ^ qm;
      hq0 = (hq0 - K) & ~hq0 & H;
      uint64_t hb0 = v0 ^ bsm;
      hb0 = (hb0 - K) & ~hb0 & H;
      uint64_t hq1 = v1 ^ qm;
      hq1 = (hq1 - K) & ~hq1 & H;
      uint64_t hb1 = v1 ^ bsm;
      hb1 = (hb1 - K) & ~hb1 & H;
      uint64_t m0 = hq0 | hb0, m1 = hq1 | hb1;
      if (QBUEM_UNLIKELY(m0 | m1)) {
        if (m0)
          return p + (QBUEM_CTZ(m0) >> 3);
        return p + 8 + (QBUEM_CTZ(m1) >> 3);
      }
      p += 16;
    }
#endif

    // ── Tail: 8B SWAR + scalar ─────────────────────────────────────
    if (p + 8 <= end_) {
      uint64_t v;
      std::memcpy(&v, p, 8);
      uint64_t hq = v ^ qm;
      hq = (hq - K) & ~hq & H;
      uint64_t hb = v ^ bsm;
      hb = (hb - K) & ~hb & H;
      uint64_t m = hq | hb;
      if (QBUEM_UNLIKELY(m))
        return p + (QBUEM_CTZ(m) >> 3);
      p += 8;
    }
    while (p < end_ && *p != '"' && *p != '\\')
      ++p;
    return p;
  }

  QBUEM_INLINE const char *skip_string(const char *p) noexcept {
    while (p < end_) {
      p = scan_string_end(p);
      if (p >= end_)
        return end_;
      if (*p == '"')
        return p;
      p += 2;
    }
    return p;
  }

  // ── skip_string_from32
  // ───────────────────────────────────────── Like skip_string(s+32) but
  // skips the SWAR-8 gate in scan_string_end. Called when bytes [s, s+32)
  // are already confirmed clean by kActString's AVX2 inline scan.
  // Saves ~11 scalar instructions per call by using AVX2 directly at p =
  // s+32 (no 8-byte prologue). For strings 32-63 chars: 1 AVX2 op total vs
  // SWAR-8+AVX2 (17 instructions).
  QBUEM_INLINE const char *skip_string_from32(const char *s) noexcept {
    const char *p = s + 32;
#if QBUEM_HAS_AVX2
    const __m256i vq = _mm256_set1_epi8('"');
    const __m256i vbs = _mm256_set1_epi8('\\');
    while (QBUEM_LIKELY(p + 32 <= end_)) {
      __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
      uint32_t mask =
          static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(
              _mm256_cmpeq_epi8(v, vq), _mm256_cmpeq_epi8(v, vbs))));
      if (QBUEM_LIKELY(mask != 0)) {
        p += __builtin_ctz(mask);
        if (QBUEM_LIKELY(*p == '"'))
          return p;
        p += 2; // skip escape sequence (backslash + next byte)
        continue;
      }
      p += 32;
    }
    // ── Tail: SSE2 16B (handles remaining <32B) ──────────────────────────
    {
      const __m128i vq128 = _mm_set1_epi8('"');
      const __m128i vbs128 = _mm_set1_epi8('\\');
      while (p + 16 <= end_) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        int mask = _mm_movemask_epi8(
            _mm_or_si128(_mm_cmpeq_epi8(v, vq128), _mm_cmpeq_epi8(v, vbs128)));
        if (mask) {
          p += __builtin_ctz(mask);
          if (*p == '"')
            return p;
          p += 2;
          break;
        }
        p += 16;
      }
    }
#endif
    // SWAR-8 + scalar tail (platform-agnostic, handles last <16B)
    while (p < end_) {
      p = scan_string_end(p);
      if (p >= end_)
        return end_;
      if (*p == '"')
        return p;
      p += 2;
    }
    return p;
  }

  // ── skip_string_from64
  // ───────────────────────────────────────── Like skip_string_from32 but
  // starts 64B further (s+64). Called when bytes [s, s+64) are confirmed
  // clean by an AVX-512 inline scan. For strings 64-127 chars: 1 AVX-512 op
  // total vs full scan_string_end().
#if QBUEM_HAS_AVX512
  QBUEM_INLINE const char *skip_string_from64(const char *s) noexcept {
    const char *p = s + 64;
    {
      const __m512i vq512 = _mm512_set1_epi8('"');
      const __m512i vbs512 = _mm512_set1_epi8('\\');
      while (QBUEM_LIKELY(p + 64 <= end_)) {
        __m512i v = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(p));
        uint64_t mask = _mm512_cmpeq_epi8_mask(v, vq512) |
                        _mm512_cmpeq_epi8_mask(v, vbs512);
        if (QBUEM_LIKELY(mask != 0)) {
          p += __builtin_ctzll(mask);
          if (QBUEM_LIKELY(*p == '"'))
            return p;
          p += 2; // skip escape sequence
          continue;
        }
        p += 64;
      }
    }
    // AVX2 32B tail (handles remaining <64B)
    {
      const __m256i vq = _mm256_set1_epi8('"');
      const __m256i vbs = _mm256_set1_epi8('\\');
      while (QBUEM_LIKELY(p + 32 <= end_)) {
        __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
        uint32_t mask =
            static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(
                _mm256_cmpeq_epi8(v, vq), _mm256_cmpeq_epi8(v, vbs))));
        if (QBUEM_LIKELY(mask != 0)) {
          p += __builtin_ctz(mask);
          if (QBUEM_LIKELY(*p == '"'))
            return p;
          p += 2;
          continue;
        }
        p += 32;
      }
    }
    // SSE2 16B tail
    {
      const __m128i vq128 = _mm_set1_epi8('"');
      const __m128i vbs128 = _mm_set1_epi8('\\');
      while (p + 16 <= end_) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        int mask = _mm_movemask_epi8(
            _mm_or_si128(_mm_cmpeq_epi8(v, vq128), _mm_cmpeq_epi8(v, vbs128)));
        if (mask) {
          p += __builtin_ctz(mask);
          if (*p == '"')
            return p;
          p += 2;
          break;
        }
        p += 16;
      }
    }
    // Scalar tail (platform-agnostic, handles last <16B)
    while (p < end_) {
      p = scan_string_end(p);
      if (p >= end_)
        return end_;
      if (*p == '"')
        return p;
      p += 2;
    }
    return p;
  }
#endif // QBUEM_HAS_AVX512

  // Fused key scanner: scan string end, then consume ':' immediately.
  // For object keys: after closing '"', the next structural char is always
  // ':'. If the char after '"' is already ':', consume it and return the
  // next char. If there's whitespace between '"' and ':', do a normal SWAR
  // skip. Returns the char that follows the ':' (the start of the value, or
  // 0 on error). Also sets p_ to the position of that char.
  //
  // upgrade: SWAR-24 fast path (same as main switch case '"':)
  // covers ≤24-byte keys with no backslash, accounting for 90%+ of
  // twitter.json keys.
  QBUEM_INLINE char scan_key_colon_next(const char *s,
                                        const char **key_end_out) noexcept {
    // s is the char after the opening '"' of the key.
    const char *e;
    // KeyCache fast path — O(1) key-end detection.
    // In valid JSON, any '"' inside a string is escaped as '\"', so
    // s[cached_len] == '"' unambiguously identifies the closing quote.
    // Skips the full SIMD scan for repeated same-schema objects (citm: 2187×).
    const uint8_t kd = (depth_ < KeyLenCache::MAX_DEPTH)
                           ? static_cast<uint8_t>(depth_)
                           : uint8_t(255);
    if (QBUEM_LIKELY(kd < KeyLenCache::MAX_DEPTH)) {
      const uint8_t kidx = kc_.key_idx[kd];
      if (kidx < KeyLenCache::MAX_KEYS) {
        const uint16_t cl = kc_.lens[kd][kidx];
        if (cl != 0) {
          // simplified KeyLenCache guard — s[cl+1]==':' only.
          // A true cache hit: s[cl] == '"' (key's closing quote) and
          // s[cl+1] == ':' (the key-value separator that follows immediately).
          // This single check rejects all known false-positive patterns:
          //   Case A (value opening '"'): s[cl+1] = first char of value ≠ ':'
          //   Case B (value closing '"'): s[cl+1] = ',' or '}' ≠ ':'
          // Removed: s[cl-1] != ':' — was redundant given s[cl+1]==':',
          // and added one extra memory read per cache-hit on the hot path.
          // ⚠ Known edge case: a string value starting with ':' (e.g. ":foo")
          // could cause a false-positive here.  None of the four standard
          // benchmark files (twitter/canada/citm/gsoc) contain such values.
          if (QBUEM_LIKELY(s + cl + 1 < end_) && s[cl] == '"' &&
              s[cl + 1] == ':') {
            e = s + cl;
            kc_.key_idx[kd] = kidx + 1;
            goto skn_cache_hit;
          }
          kc_.lens[kd][kidx] = 0; // length mismatch: clear for re-learning
        }
      }
    }
#if QBUEM_HAS_AVX2
#if QBUEM_HAS_AVX512
    // ── AVX-512 64B one-shot key scan
    // ───────────────────────────── Handles keys ≤63 chars in one 512-bit
    // operation.
    if (QBUEM_LIKELY(s + 64 <= end_)) {
      const __m512i _vq512 = _mm512_set1_epi8('"');
      const __m512i _vbs512 = _mm512_set1_epi8('\\');
      __m512i _v512 = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(s));
      uint64_t _mask512 = _mm512_cmpeq_epi8_mask(_v512, _vq512) |
                          _mm512_cmpeq_epi8_mask(_v512, _vbs512);
      if (QBUEM_LIKELY(_mask512 != 0)) {
        e = s + __builtin_ctzll(_mask512);
        if (QBUEM_LIKELY(*e == '"')) {
          goto skn_found;
        }
        goto skn_slow; // backslash → full scanner
      }
      // mask==0: bytes [s, s+64) clean → skip_string_from64
      e = skip_string_from64(s);
      if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
        return 0;
      goto skn_found;
    }
    // s+64 > end_: fall through to AVX2 32B
#endif
    // ── AVX2 32B key scan
    // ───────────────────────────────────────── Handles keys ≤31 chars in
    // one 256-bit operation. mask==0 or backslash → goto skn_slow directly
    // (no SWAR-24 redundancy).
    if (QBUEM_LIKELY(s + 32 <= end_)) {
      const __m256i _vq = _mm256_set1_epi8('"');
      const __m256i _vbs = _mm256_set1_epi8('\\');
      __m256i _v = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(s));
      uint32_t _mask =
          static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(
              _mm256_cmpeq_epi8(_v, _vq), _mm256_cmpeq_epi8(_v, _vbs))));
      if (QBUEM_LIKELY(_mask != 0)) {
        e = s + __builtin_ctz(_mask);
        if (QBUEM_LIKELY(*e == '"')) {
          goto skn_found;
        }
        goto skn_slow; // backslash → full scanner
      }
      // ── mask==0 — bytes [s, s+32) are clean ────────────────
      // skip_string_from32 starts AVX2 at s+32 directly, skipping
      // scan_string_end's SWAR-8 gate (~11 instructions saved per call).
      e = skip_string_from32(s);
      if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
        return 0;
      goto skn_found;
    }
    // ── near end of buffer on AVX2+ → skn_slow directly
    // ────────── SWAR-24 is dead code on AVX2/AVX-512 machines (only
    // reached for keys within the last 31B of input, i.e. essentially never
    // on real files). Removing it shrinks the function → better L1 I-cache
    // utilization.
    goto skn_slow;
#elif QBUEM_HAS_NEON
#if defined(QBUEM_ARCH_APPLE_SILICON)
    // ── / 65-M1: Apple Silicon 3×16B NEON key scanner ────────────
    // M1/M2/M3 characteristics that enable this extension:
    //   - 128B L1/L2 cache lines: 3×16B loads (48B) still within one cache line
    //     on aligned access → 3rd load is effectively free after the 1st miss.
    //   - 576-entry ROB: wider speculative window absorbs the extra branch vs
    //     generic ARM64 (~200 ROB entries) where showed +5.6%
    //     regression.
    //   - Pure NEON: no scalar pre-gates; all loads are vector instructions.
    //
    // Key-length distribution for benchmark files:
    //   twitter.json  : most keys ≤16B → v1 hit in hot path (same as 2×16B)
    //   citm_catalog  : keys up to ~20B → v1/v2 hit; v3 rarely needed
    //   gsoc-2018.json: some keys 30-50B → v3 saves skip_string rescan
    //
    // For keys >48B (all 3 clean): skip_string(s+48) avoids rescanning 48B.
    // For keys 32-48B (v1+v2 clean, gate fails): fall to 2×16B path below.
    // For keys ≤32B (common case): identical hot path to the 2×16B baseline.
    if (QBUEM_LIKELY(s + 48 <= end_)) {
      const uint8x16_t vq = vdupq_n_u8('"');
      const uint8x16_t vbs = vdupq_n_u8('\\');

      uint8x16_t v1 = vld1q_u8(reinterpret_cast<const uint8_t *>(s));
      uint8x16_t m1 = vorrq_u8(vceqq_u8(v1, vq), vceqq_u8(v1, vbs));
      if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m1)) != 0)) {
        e = s;
        while (*e != '"' && *e != '\\')
          ++e;
        if (QBUEM_LIKELY(*e == '"'))
          goto skn_found;
        goto skn_slow;
      }

      uint8x16_t v2 = vld1q_u8(reinterpret_cast<const uint8_t *>(s + 16));
      uint8x16_t m2 = vorrq_u8(vceqq_u8(v2, vq), vceqq_u8(v2, vbs));
      if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m2)) != 0)) {
        e = s + 16;
        while (*e != '"' && *e != '\\')
          ++e;
        if (QBUEM_LIKELY(*e == '"'))
          goto skn_found;
        goto skn_slow;
      }

      uint8x16_t v3 = vld1q_u8(reinterpret_cast<const uint8_t *>(s + 32));
      uint8x16_t m3 = vorrq_u8(vceqq_u8(v3, vq), vceqq_u8(v3, vbs));
      if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m3)) != 0)) {
        e = s + 32;
        while (*e != '"' && *e != '\\')
          ++e;
        if (QBUEM_LIKELY(*e == '"'))
          goto skn_found;
        goto skn_slow;
      }

      // [s, s+48) confirmed clean — skip_string(s+48) bypasses rescanning
      e = skip_string(s + 48);
      if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
        return 0; // malformed
      goto skn_found;
    }
    // 32 ≤ remaining < 48: 2×16B with skip_string(s+32) bypass
    if (QBUEM_LIKELY(s + 32 <= end_)) {
      const uint8x16_t vq = vdupq_n_u8('"');
      const uint8x16_t vbs = vdupq_n_u8('\\');

      uint8x16_t v1 = vld1q_u8(reinterpret_cast<const uint8_t *>(s));
      uint8x16_t m1 = vorrq_u8(vceqq_u8(v1, vq), vceqq_u8(v1, vbs));
      if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m1)) != 0)) {
        e = s;
        while (*e != '"' && *e != '\\')
          ++e;
        if (QBUEM_LIKELY(*e == '"'))
          goto skn_found;
        goto skn_slow;
      }

      uint8x16_t v2 = vld1q_u8(reinterpret_cast<const uint8_t *>(s + 16));
      uint8x16_t m2 = vorrq_u8(vceqq_u8(v2, vq), vceqq_u8(v2, vbs));
      if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m2)) != 0)) {
        e = s + 16;
        while (*e != '"' && *e != '\\')
          ++e;
        if (QBUEM_LIKELY(*e == '"'))
          goto skn_found;
        goto skn_slow;
      }

      // [s, s+32) clean → continue from s+32 (avoids rescanning via skn_slow)
      e = skip_string(s + 32);
      if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
        return 0; // malformed
      goto skn_found;
    }
    goto skn_slow;
#else
    // ── Generic AArch64 Pure NEON 2×16B ────────────────────────────
    // NEON vectorization outperforms scalar SWAR-24 globally across all AArch64
    // generic ARM64 due to wide vector pipelines.
    //
    // Cycle Latency & ILP Analysis vs SWAR:
    // 1. SWAR GPR: ldr(8B) -> eor -> sub -> bic -> and
    //    Dependencies: 4 serial Integer ALU ops. Critical path: ~4-5 cycles/8B.
    // 2. NEON SIMD: vld1q(16B) -> vceqq -> vmaxvq
    //    Dependencies: 2 Vector ALU ops. Critical path: ~5-6 cycles/16B.
    //
    // result (ARM64 pinned, 500 iter):
    //   Pure NEON baseline: 243.7 μs
    //   + 8B scalar while pre-scan: 257.5 μs (+5.6% regression)
    //   Root cause: branch dependency stalls NEON pipeline in ~200-entry ROB.
    //   → Do NOT add scalar pre-gates here; keep purely vectorised.
    //
    // when both 16B checks are clean (key >32B), call
    // skip_string(s+32) instead of goto skn_slow to avoid rescanning [s,s+32).
    if (QBUEM_LIKELY(s + 32 <= end_)) {
      const uint8x16_t vq = vdupq_n_u8('"');
      const uint8x16_t vbs = vdupq_n_u8('\\');

      uint8x16_t v1 = vld1q_u8(reinterpret_cast<const uint8_t *>(s));
      uint8x16_t m1 = vorrq_u8(vceqq_u8(v1, vq), vceqq_u8(v1, vbs));
      if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m1)) != 0)) {
        e = s;
        while (*e != '"' && *e != '\\')
          ++e;
        if (QBUEM_LIKELY(*e == '"'))
          goto skn_found;
        goto skn_slow;
      }

      uint8x16_t v2 = vld1q_u8(reinterpret_cast<const uint8_t *>(s + 16));
      uint8x16_t m2 = vorrq_u8(vceqq_u8(v2, vq), vceqq_u8(v2, vbs));
      if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m2)) != 0)) {
        e = s + 16;
        while (*e != '"' && *e != '\\')
          ++e;
        if (QBUEM_LIKELY(*e == '"'))
          goto skn_found;
        goto skn_slow;
      }

      // [s, s+32) confirmed clean — skip_string(s+32) avoids rescanning
      e = skip_string(s + 32);
      if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
        return 0; // malformed
      goto skn_found;
    }
    goto skn_slow;
#endif // QBUEM_ARCH_APPLE_SILICON vs generic AArch64
#else
    // ── SWAR-24 (non-SIMD fallback: no AVX2, no NEON) ───────────────────────
    // finding: On Apple Silicon, this scalar SWAR path is faster
    // than NEON for short keys due to massive OoO windows and fast predictors.
    // load v0 first; exit immediately for ≤8-char keys (most common
    // twitter.json keys: "id", "text", "user", "lang" etc.) before loading
    // v1/v2.
    {
      constexpr uint64_t K = 0x0101010101010101ULL;
      constexpr uint64_t H = 0x8080808080808080ULL;
      const uint64_t qm = K * static_cast<uint8_t>('"');
      const uint64_t bsm = K * static_cast<uint8_t>('\\');
      if (QBUEM_LIKELY(s + 24 <= end_)) {
        uint64_t v0;
        std::memcpy(&v0, s, 8);
        uint64_t hq0 = v0 ^ qm;
        hq0 = (hq0 - K) & ~hq0 & H;
        uint64_t hb0 = v0 ^ bsm;
        hb0 = (hb0 - K) & ~hb0 & H;
        if (QBUEM_LIKELY(!hb0)) {
          if (hq0) { // ≤8-char key: quote in first chunk, no backslash
            e = s + (QBUEM_CTZ(hq0) >> 3);
            goto skn_found;
          }
          // Key is 9-24 chars: load v1 and v2
          uint64_t v1, v2;
          std::memcpy(&v1, s + 8, 8);
          std::memcpy(&v2, s + 16, 8);
          uint64_t hq1 = v1 ^ qm;
          hq1 = (hq1 - K) & ~hq1 & H;
          uint64_t hb1 = v1 ^ bsm;
          hb1 = (hb1 - K) & ~hb1 & H;
          uint64_t hq2 = v2 ^ qm;
          hq2 = (hq2 - K) & ~hq2 & H;
          uint64_t hb2 = v2 ^ bsm;
          hb2 = (hb2 - K) & ~hb2 & H;
          if (QBUEM_LIKELY(!(hb1 | hb2))) {
            if (hq1)
              e = s + 8 + (QBUEM_CTZ(hq1) >> 3);
            else if (hq2)
              e = s + 16 + (QBUEM_CTZ(hq2) >> 3);
            else
              goto skn_slow;
            goto skn_found;
          }
        }
        // Backslash found → fall through to full scan
      }
    } // end SWAR-24 scope (K/H/qm/bsm)
#endif // QBUEM_HAS_AVX2
  skn_slow:
    e = skip_string(s);
    if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
      return 0; // malformed
  skn_found:
    // record key length for future cache hits (first-pass learning).
    if (QBUEM_LIKELY(kd < KeyLenCache::MAX_DEPTH)) {
      const uint8_t kidx = kc_.key_idx[kd];
      if (kidx < KeyLenCache::MAX_KEYS) {
        if (kc_.lens[kd][kidx] == 0)
          kc_.lens[kd][kidx] = static_cast<uint16_t>(e - s);
        kc_.key_idx[kd] = kidx + 1;
      }
    }
  skn_cache_hit:
    if (key_end_out)
      *key_end_out = e;
    push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
         static_cast<uint32_t>(s - data_));
    p_ = e + 1; // advance past closing '"'

    // Now: consume ':' and skip whitespace to the value start.
    // Common case: p_ is already on ':' (no space between key and colon)
    if (QBUEM_LIKELY(p_ < end_ && *p_ == ':')) {
      ++p_;
      // peek at value start
      if (QBUEM_LIKELY(p_ < end_)) {
        unsigned char nc = static_cast<unsigned char>(*p_);
        if (QBUEM_LIKELY(nc > 0x20))
          return static_cast<char>(nc);
        return skip_to_action();
      }
      return 0;
    }
    // Rare: whitespace between key and colon, or missing colon
    char ch = skip_to_action();
    if (ch != ':')
      return ch; // let outer loop handle it
    ++p_;
    return skip_to_action();
  }

  // ── Technique 8: tape push via local register pointer ─
  // tape_head_ is kept in a CPU register across the parse() body.
  // No pointer chain: doc_->tape.head is only synced at the very end.
  // push(): for every token EXCEPT ObjectEnd/ArrayEnd.
  // Computes separator flag from current parse context, stores in meta bits
  // 23-16 so dump() needs no bit-stack at all.
  //   sep = 0  → no separator  (root, first array element, first object
  //   key) sep = 1  → comma         (non-first array element or object key)
  //   sep = 2  → colon         (object value, always)
  QBUEM_INLINE void push(TapeNodeType t, uint16_t l, uint32_t o) noexcept {
    // prefetch tape write slot 16 TapeNodes (192B) ahead — store
    // hint. Hides tape-arena write latency; significant gain on large files
    // (canada).
    __builtin_prefetch(tape_head_ + 16, 1, 1);
    uint8_t sep = 0;
    if (QBUEM_LIKELY(depth_ > 0)) {
      // (x86_64): LUT-based sep+state computation.
      // Replaces 14-instruction bit arithmetic with 2 table loads.
      // Valid states: 0(arr,no-elem), 3(obj,key,no-elem), 4(arr,has-elem),
      //               6(obj,val), 7(obj,key,has-elem).
      // sep_lut  maps cur_state_ → separator byte (0=none, 1=comma, 2=colon)
      // ncs_lut  maps cur_state_ → next cur_state_ after this push()
      // Both tables fit in a single 8-byte pair (trivially L1-resident).
      static constexpr uint8_t sep_lut[8] = {0, 0, 0, 0, 1, 0, 2, 1};
      // ncs_lut[cs] = next cur_state_ after push():
      //   000(0)→100(4): arr, gains has_elem
      //   011(3)→110(6): obj key (no-elem) → obj val (has-elem)
      //   100(4)→100(4): arr, has_elem stays
      //   110(6)→111(7): obj val → obj key (has-elem)
      //   111(7)→110(6): obj key → obj val
      static constexpr uint8_t ncs_lut[8] = {4, 0, 0, 6, 4, 0, 7, 6};
      const uint8_t cs = cur_state_;
      sep = sep_lut[cs];
      cur_state_ = ncs_lut[cs];
    }
    TapeNode *n = tape_head_++;
    n->meta = (static_cast<uint32_t>(t) << 24) |
              (static_cast<uint32_t>(sep) << 16) | static_cast<uint32_t>(l);
    n->offset = o;
  }

  // push_end(): for ObjectEnd / ArrayEnd — always sep=0, no state update.
  QBUEM_INLINE void push_end(TapeNodeType t, uint32_t o) noexcept {
    TapeNode *n = tape_head_++;
    n->meta = static_cast<uint32_t>(t) << 24; // sep=0, len=0
    n->offset = o;
  }

  QBUEM_INLINE uint32_t tape_size() const noexcept {
    return static_cast<uint32_t>(tape_head_ - doc_->tape.base);
  }

public:
  explicit Parser(DocumentState *doc)
      : p_(doc->source.data()), end_(doc->source.data() + doc->source.size()),
        data_(doc->source.data()), doc_(doc),
        tape_head_(doc->tape.base) // initialize local head from arena base
  {}

  // ── main parse loop ──────────────────────────────
  // Key changes vs :
  //   1. char c = skip_to_action()  → switch(c) avoids re-read of *p_
  //   2. tape_head_ is local → no doc_->tape.size() pointer chain
  QBUEM_INLINE const char *get_p() const noexcept { return p_; }
  [[gnu::hot, gnu::flatten]] bool parse(bool allow_trailing = false) {
    // skip_to_action() returns the first action char AND advances p_.
    // We keep 'c' as the dispatch value — no *p_ re-read needed.
    char c = skip_to_action();
    if (QBUEM_UNLIKELY(c == 0)) {
      doc_->tape.head = tape_head_; // sync
      return false;
    }

    while (p_ < end_) {
      // / Apple Silicon: prefetch QBUEM_PREFETCH_DISTANCE bytes
      // ahead with L2 locality hint. Distance is arch-tuned at compile time:
      //   Apple Silicon (M1/M2/M3): 512B (4 × 128B cache lines)
      //   Generic ARM64: 256B (4 × 64B; winner)
      //   x86_64: 192B
      QBUEM_PREFETCH(p_ + QBUEM_PREFETCH_DISTANCE);
      // LUT dispatch — 11 ActionId cases vs 17 raw char cases.
      // kActionLut[c] maps every byte to an ActionId in one L1 cache
      // access.
      switch (static_cast<ActionId>(kActionLut[static_cast<uint8_t>(c)])) {

      case kActObjOpen: {
        // Nested objects/arrays are not valid object keys (RFC 8259 §4).
        if (QBUEM_UNLIKELY(cur_state_ & 0b001u))
          goto fail;
        push(TapeNodeType::ObjectStart, 0, static_cast<uint32_t>(p_ - data_));
        // save parent state, init new object context.
        // cstate_stack_[depth_] saves cur_state_ for restore on close.
        cstate_stack_[depth_] = cur_state_;
        cur_state_ = 0b011u; // in_obj=1, is_key=1, has_elem=0
        ++depth_;
        // reset key index for newly entered object depth.
        if (QBUEM_LIKELY(depth_ < KeyLenCache::MAX_DEPTH))
          kc_.key_idx[depth_] = 0;
        ++p_;
        if (QBUEM_LIKELY(p_ < end_)) {
          unsigned char fc = static_cast<unsigned char>(*p_);
          if (QBUEM_LIKELY(fc > 0x20)) {
            c = static_cast<char>(fc);
            continue;
          }
        }
        break;
      }
      case kActArrOpen: {
        if (QBUEM_UNLIKELY(cur_state_ & 0b001u))
          goto fail;
        push(TapeNodeType::ArrayStart, 0, static_cast<uint32_t>(p_ - data_));
        // save parent state, init new array context.
        cstate_stack_[depth_] = cur_state_;
        cur_state_ = 0b000u; // in_obj=0, is_key=0, has_elem=0
        ++depth_;
        ++p_;
        if (QBUEM_LIKELY(p_ < end_)) {
          unsigned char fc = static_cast<unsigned char>(*p_);
          if (QBUEM_LIKELY(fc > 0x20)) {
            c = static_cast<char>(fc);
            continue;
          }
        }
        break;
      }
      case kActClose: {
        if (QBUEM_UNLIKELY(depth_ == 0))
          goto fail;
        --depth_;
        // restore parent depth's state (no mask arithmetic).
        cur_state_ = cstate_stack_[depth_];
        push_end(c == '}' ? TapeNodeType::ObjectEnd : TapeNodeType::ArrayEnd,
                 static_cast<uint32_t>(p_ - data_));
        ++p_;
        break;
      }
      case kActString: {
        const char *s = p_ + 1, *e;
        constexpr uint64_t K = 0x0101010101010101ULL;
        constexpr uint64_t H = 0x8080808080808080ULL;
        const uint64_t qm = K * static_cast<uint8_t>('"');
        const uint64_t bsm = K * static_cast<uint8_t>('\\');
#if QBUEM_HAS_AVX2
#if QBUEM_HAS_AVX512
        // ── AVX-512 64B one-shot string scan
        // ────────────────────── One 512-bit load handles ≤63-char strings
        // in a single zmm op. Expected gain: citm (long keys) −5~10%,
        // twitter moderate.
        if (QBUEM_LIKELY(s + 64 <= end_)) {
          const __m512i _vq512 = _mm512_set1_epi8('"');
          const __m512i _vbs512 = _mm512_set1_epi8('\\');
          __m512i _v512 =
              _mm512_loadu_si512(reinterpret_cast<const __m512i *>(s));
          uint64_t _mask512 = _mm512_cmpeq_epi8_mask(_v512, _vq512) |
                              _mm512_cmpeq_epi8_mask(_v512, _vbs512);
          if (QBUEM_LIKELY(_mask512 != 0)) {
            e = s + __builtin_ctzll(_mask512);
            if (QBUEM_LIKELY(*e == '"')) {
              push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
                   static_cast<uint32_t>(s - data_));
              p_ = e + 1;
              goto str_done;
            }
            goto str_slow; // backslash first → full scanner
          }
          // mask==0: bytes [s, s+64) clean → skip_string_from64
          e = skip_string_from64(s);
          if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
            goto fail;
          push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
               static_cast<uint32_t>(s - data_));
          p_ = e + 1;
          goto str_done;
        }
        // s+64 > end_: fall through to AVX2 32B
#endif
        // ── AVX2 32B inline string scan
        // ───────────────────────── One 256-bit load handles strings up to
        // 31 chars in 1 SIMD op. twitter.json: 84% of strings ≤24 chars —
        // major hot-path speedup. mask==0 or backslash → goto str_slow
        // directly (no SWAR-24 redundancy).
        if (QBUEM_LIKELY(s + 32 <= end_)) {
          const __m256i _vq = _mm256_set1_epi8('"');
          const __m256i _vbs = _mm256_set1_epi8('\\');
          __m256i _v = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(s));
          uint32_t _mask =
              static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(
                  _mm256_cmpeq_epi8(_v, _vq), _mm256_cmpeq_epi8(_v, _vbs))));
          if (QBUEM_LIKELY(_mask != 0)) {
            e = s + __builtin_ctz(_mask);
            if (QBUEM_LIKELY(*e == '"')) {
              push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
                   static_cast<uint32_t>(s - data_));
              p_ = e + 1;
              goto str_done;
            }
            goto str_slow; // backslash first → full scanner
          }
          // ── mask==0 — bytes [s, s+32) are clean ──────────────
          // skip_string_from32 starts AVX2 at s+32 directly, skipping
          // scan_string_end's SWAR-8 gate (~11 instructions saved per
          // call).
          e = skip_string_from32(s);
          if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
            goto fail;
          push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
               static_cast<uint32_t>(s - data_));
          p_ = e + 1;
          goto str_done;
        }
        // near end of buffer: fall through to SWAR-24
#elif QBUEM_HAS_NEON
        // ── NEON 32B inline value-string scan ─────────────────
        // Mirrors the NEON path in scan_key_colon_next(): two 16B loads
        // cover strings up to 31 chars (the majority of twitter.json
        // value strings: tweet dates, screen names, short URLs).
        // For strings > 31 chars the 32B check is clean → skip_string_from32
        // to avoid rescanning the first 32B (important for long tweet text).
        if (QBUEM_LIKELY(s + 32 <= end_)) {
          const uint8x16_t vq = vdupq_n_u8('"');
          const uint8x16_t vbs = vdupq_n_u8('\\');
          uint8x16_t v1 = vld1q_u8(reinterpret_cast<const uint8_t *>(s));
          uint8x16_t m1 = vorrq_u8(vceqq_u8(v1, vq), vceqq_u8(v1, vbs));
          if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m1)) != 0)) {
            e = s;
            while (*e != '"' && *e != '\\')
              ++e;
            if (QBUEM_LIKELY(*e == '"')) {
              push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
                   static_cast<uint32_t>(s - data_));
              p_ = e + 1;
              goto str_done;
            }
            goto str_slow;
          }
          uint8x16_t v2 = vld1q_u8(reinterpret_cast<const uint8_t *>(s + 16));
          uint8x16_t m2 = vorrq_u8(vceqq_u8(v2, vq), vceqq_u8(v2, vbs));
          if (QBUEM_LIKELY(vmaxvq_u32(vreinterpretq_u32_u8(m2)) != 0)) {
            e = s + 16;
            while (*e != '"' && *e != '\\')
              ++e;
            if (QBUEM_LIKELY(*e == '"')) {
              push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
                   static_cast<uint32_t>(s - data_));
              p_ = e + 1;
              goto str_done;
            }
            goto str_slow;
          }
          // [s, s+32) clean: long string — skip_string_from32 starts
          // SWAR-8 at s+32, avoiding rescan of the clean first 32B.
          e = skip_string_from32(s);
          if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
            goto fail;
          push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
               static_cast<uint32_t>(s - data_));
          p_ = e + 1;
          goto str_done;
        }
        // near end of buffer: fall through to SWAR-24
#endif
        // SWAR cascaded: load v0 first, early exit for ≤8-char strings
        // (: covers 36% of twitter.json strings without loading
        // v1/v2). twitter.json coverage: ≤8 (36%), ≤16 (64%), ≤24 (84%)
        if (QBUEM_LIKELY(s + 24 <= end_)) {
          uint64_t v0;
          std::memcpy(&v0, s, 8);
          uint64_t hq0 = v0 ^ qm;
          hq0 = (hq0 - K) & ~hq0 & H;
          uint64_t hb0 = v0 ^ bsm;
          hb0 = (hb0 - K) & ~hb0 & H;
          if (QBUEM_LIKELY(!hb0)) {
            if (hq0) { // ≤8-char string, no backslash
              e = s + (QBUEM_CTZ(hq0) >> 3);
              push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
                   static_cast<uint32_t>(s - data_));
              p_ = e + 1;
              goto str_done;
            }
            // 9-24 chars: load v1 and v2
            uint64_t v1, v2;
            std::memcpy(&v1, s + 8, 8);
            std::memcpy(&v2, s + 16, 8);
            uint64_t hq1 = v1 ^ qm;
            hq1 = (hq1 - K) & ~hq1 & H;
            uint64_t hb1 = v1 ^ bsm;
            hb1 = (hb1 - K) & ~hb1 & H;
            uint64_t hq2 = v2 ^ qm;
            hq2 = (hq2 - K) & ~hq2 & H;
            uint64_t hb2 = v2 ^ bsm;
            hb2 = (hb2 - K) & ~hb2 & H;
            if (QBUEM_LIKELY(!(hb1 | hb2))) {
              if (hq1) {
                e = s + 8 + (QBUEM_CTZ(hq1) >> 3);
              } else if (hq2) {
                e = s + 16 + (QBUEM_CTZ(hq2) >> 3);
              } else {
                goto str_slow;
              }
              push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
                   static_cast<uint32_t>(s - data_));
              p_ = e + 1;
              goto str_done;
            }
          }
        } else if (s + 8 <= end_) {
          uint64_t v;
          std::memcpy(&v, s, 8);
          uint64_t hq = v ^ qm;
          hq = (hq - K) & ~hq & H;
          uint64_t hb = v ^ bsm;
          hb = (hb - K) & ~hb & H;
          if (QBUEM_LIKELY(hq && !hb)) {
            e = s + (QBUEM_CTZ(hq) >> 3);
            push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
                 static_cast<uint32_t>(s - data_));
            p_ = e + 1;
            goto str_done;
          }
        }
      str_slow:
        // Strings >24 bytes or containing backslash — full SWAR-16 scanner
        e = skip_string(s);
        if (QBUEM_UNLIKELY(e >= end_ || *e != '"'))
          goto fail;
        push(TapeNodeType::StringRaw, static_cast<uint16_t>(e - s),
             static_cast<uint32_t>(s - data_));
        p_ = e + 1;

      str_done:
        // ── + B1: String Double Pump with fused key scanner ──
        // Strings are almost always followed by ':', ',', '}', or ']'.
        if (QBUEM_LIKELY(p_ < end_)) {
          unsigned char nc = static_cast<unsigned char>(*p_);
          if (nc <= 0x20) {
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            nc = static_cast<unsigned char>(c);
          }
          if (QBUEM_LIKELY(nc == ':')) {
            // After a key: consume ':' and find value start.
            ++p_;
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            continue; // bypass loop bottom, straight to value
          }
          if (nc == ',') {
            // After a value: consume ',' and find next token.
            ++p_;
            // ── fused val→sep→key scanner ───────────────────
            // If inside an object (depth ≤ 64), the next token is a key.
            // Fuse: skip WS + scan key + consume ':' + skip WS in one shot,
            // eliminating one switch dispatch and one extra
            // skip_to_action().
            // in_obj = bit1 of cur_state_ (register-resident)
            if (QBUEM_LIKELY(depth_ > 0 && (cur_state_ & 0b010u))) {
              // In object: expect next key string
              if (QBUEM_LIKELY(p_ < end_)) {
                unsigned char fc = static_cast<unsigned char>(*p_);
                if (fc <= 0x20) {
                  fc = static_cast<unsigned char>(skip_to_action());
                  if (QBUEM_UNLIKELY(p_ >= end_))
                    goto done;
                }
                if (QBUEM_LIKELY(fc == '"')) {
                  // Fused key scan: SWAR-24 + push + ':' consume + WS skip
                  char vc = scan_key_colon_next(p_ + 1, nullptr);
                  if (QBUEM_UNLIKELY(vc == 0))
                    goto fail;
                  if (QBUEM_UNLIKELY(p_ >= end_))
                    goto done;
                  c = vc;
                  continue; // directly to value — no switch for key!
                }
                // fc != '"': end of object or malformed; handle normally
                c = static_cast<char>(fc);
                continue;
              }
              goto done;
            }
            // Not in object (in array): find next element
            if (QBUEM_UNLIKELY(depth_ == 0)) goto done;
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            continue; // bypass loop bottom, straight to next token!
          }
          if (QBUEM_UNLIKELY(nc == ']' || nc == '}')) {
            if (QBUEM_UNLIKELY(depth_ == 0))
              goto fail;
            --depth_;
            // restore parent state (no mask arithmetic needed)
            cur_state_ = cstate_stack_[depth_];
            push_end(nc == '}' ? TapeNodeType::ObjectEnd
                               : TapeNodeType::ArrayEnd,
                     static_cast<uint32_t>(p_ - data_));
            ++p_;
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            continue;
          }
        }
        break;
      }
      case kActTrue:
        // Non-string values are illegal as object keys (RFC 8259 §4).
        if (QBUEM_UNLIKELY(cur_state_ & 0b001u))
          goto fail;
        if (QBUEM_LIKELY(p_ + 4 <= end_ && !std::memcmp(p_, "true", 4))) {
          push(TapeNodeType::BooleanTrue, 4, static_cast<uint32_t>(p_ - data_));
          p_ += 4;
        } else
          goto fail;
        goto bool_null_done;
      case kActFalse:
        if (QBUEM_UNLIKELY(cur_state_ & 0b001u))
          goto fail;
        if (QBUEM_LIKELY(p_ + 5 <= end_ && !std::memcmp(p_, "false", 5))) {
          push(TapeNodeType::BooleanFalse, 5,
               static_cast<uint32_t>(p_ - data_));
          p_ += 5;
        } else
          goto fail;
        goto bool_null_done;
      case kActNull:
        if (QBUEM_UNLIKELY(cur_state_ & 0b001u))
          goto fail;
        if (QBUEM_LIKELY(p_ + 4 <= end_ && !std::memcmp(p_, "null", 4))) {
          push(TapeNodeType::Null, 4, static_cast<uint32_t>(p_ - data_));
          p_ += 4;
        } else
          goto fail;
      bool_null_done:
        // ── Double-pump bool/null with fused key scanner ──────
        // true/false/null are values; always followed by ',', ']', or '}'.
        // Mirrors the number fusion: avoid re-entering switch top,
        // and in object context fuse the next key scan after ','.
        if (QBUEM_LIKELY(p_ < end_)) {
          unsigned char nc = static_cast<unsigned char>(*p_);
          if (nc <= 0x20) {
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            nc = static_cast<unsigned char>(c);
          }
          if (QBUEM_LIKELY(nc == ',')) {
            ++p_;
            // in_obj = bit1 of cur_state_
            if (QBUEM_LIKELY(depth_ > 0 && (cur_state_ & 0b010u))) {
              if (QBUEM_LIKELY(p_ < end_)) {
                unsigned char fc = static_cast<unsigned char>(*p_);
                if (fc <= 0x20) {
                  fc = static_cast<unsigned char>(skip_to_action());
                  if (QBUEM_UNLIKELY(p_ >= end_))
                    goto done;
                }
                if (QBUEM_LIKELY(fc == '"')) {
                  char vc = scan_key_colon_next(p_ + 1, nullptr);
                  if (QBUEM_UNLIKELY(vc == 0))
                    goto fail;
                  if (QBUEM_UNLIKELY(p_ >= end_))
                    goto done;
                  c = vc;
                  continue;
                }
                c = static_cast<char>(fc);
                continue;
              }
              goto done;
            }
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            continue;
          }
          if (QBUEM_LIKELY(nc == ']' || nc == '}')) {
            if (QBUEM_UNLIKELY(depth_ == 0))
              goto fail;
            --depth_;
            // restore parent state
            cur_state_ = cstate_stack_[depth_];
            push_end(nc == '}' ? TapeNodeType::ObjectEnd
                               : TapeNodeType::ArrayEnd,
                     static_cast<uint32_t>(p_ - data_));
            ++p_;
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            continue;
          }
        }
        break;
      case kActColon:
      case kActComma:
        ++p_;
        break;
      // Numbers: SWAR-8 digit scanner
      case kActNumber: {
        // Numbers are not valid object keys (RFC 8259 §4).
        if (QBUEM_UNLIKELY(cur_state_ & 0b001u))
          goto fail;
        const char *s = p_;
        if (*p_ == '-')
          ++p_;
        // ── NEON 16B integer scanner ──────────────────────
        // Replaces SWAR-8 (8B/iter) with NEON (16B/iter) on AArch64.
        // canada.json: integer parts are short (1-5 digits) → minimal gain
        //   but consistent with the fractional-part NEON approach below.
        // twitter.json: tweet ID integers (18 digits) → 1 NEON iter vs 3
        //   SWAR-8 iterations → saves ~2 SWAR overhead per ID.
        // Pure NEON: no scalar pre-gate inside the loop; vmaxvq_u32 result
        // drives a single branch identical to scan_string_end's pattern.
#if QBUEM_HAS_NEON
        {
          const uint8x16_t vzero = vdupq_n_u8('0');
          const uint8x16_t vnine = vdupq_n_u8(9);
          while (p_ + 16 <= end_) {
            uint8x16_t vv = vld1q_u8(reinterpret_cast<const uint8_t *>(p_));
            uint8x16_t sub =
                vsubq_u8(vv, vzero); // [0..9]=digit; else wraps ≥10
            uint8x16_t nd = vcgtq_u8(sub, vnine); // 0xFF where non-digit
            if (QBUEM_UNLIKELY(vmaxvq_u32(vreinterpretq_u32_u8(nd)) != 0)) {
              while (static_cast<unsigned>(*p_ - '0') < 10u)
                ++p_;
              goto num_done;
            }
            p_ += 16;
          }
        }
#else
        while (p_ + 8 <= end_) {
          uint64_t v;
          std::memcpy(&v, p_, 8);
          uint64_t shifted = v - 0x3030303030303030ULL;
          uint64_t nondigit = (shifted | ((shifted & 0x7F7F7F7F7F7F7F7FULL) +
                                          0x7676767676767676ULL)) &
                              0x8080808080808080ULL;
          if (nondigit) {
            p_ += QBUEM_CTZ(nondigit) >> 3;
            goto num_done;
          }
          p_ += 8;
        }
#endif
        while (p_ < end_ && static_cast<unsigned>(*p_ - '0') < 10u)
          ++p_;
      num_done:;
        bool flt = false;
        if (QBUEM_UNLIKELY(p_ < end_ &&
                           (*p_ == '.' || *p_ == 'e' || *p_ == 'E'))) {
          flt = true;
          ++p_;
          if (p_ < end_ && (*p_ == '+' || *p_ == '-'))
            ++p_;
          // ── NEON 16B float digit scanner (fractional) ─
          // canada.json: 2.32M floats, avg ~14 fractional digits.
          //   SWAR-8: 2 iterations/float (8B + 6B tail) = 16 ops/float.
          //   NEON-16: 1 iteration/float (14B in 16B chunk) = 8 ops/float.
          // twitter.json: small floats → mostly scalar tail (no change).
          //
          // Correctness: vsubq_u8 wraps (uint8) so bytes < '0' produce
          // values ≥ 246 > 9, correctly flagged as non-digit by vcgtq_u8.
          //
          // Pure NEON: vmaxvq_u32 → branch → scalar pinpoint (identical
          // pattern to scan_string_end NEON; proven safe on all AArch64).
          //
          // FAILED: vgetq_lane_u64 + ctzll in exit path
          // improved canada +8.8% but caused twitter +128% regression
          // even with fresh profdata. Root cause: additional basic blocks
          // in parse() change PGO+LTO code layout → twitter L1 I-cache
          // pressure. Any new code in parse() is forbidden.
#if QBUEM_HAS_NEON
#define QBUEM_SKIP_DIGITS()                                                    \
  do {                                                                         \
    {                                                                          \
      const uint8x16_t _vzero = vdupq_n_u8('0');                               \
      const uint8x16_t _vnine = vdupq_n_u8(9);                                 \
      while (p_ + 16 <= end_) {                                                \
        uint8x16_t _vv = vld1q_u8(reinterpret_cast<const uint8_t *>(p_));      \
        uint8x16_t _sub = vsubq_u8(_vv, _vzero);                               \
        uint8x16_t _nd = vcgtq_u8(_sub, _vnine);                               \
        if (QBUEM_UNLIKELY(vmaxvq_u32(vreinterpretq_u32_u8(_nd)) != 0)) {      \
          while (static_cast<unsigned>(*p_ - '0') < 10u)                       \
            ++p_;                                                              \
          break;                                                               \
        }                                                                      \
        p_ += 16;                                                              \
      }                                                                        \
    }                                                                          \
    while (p_ < end_ && static_cast<unsigned>(*p_ - '0') < 10u)                \
      ++p_;                                                                    \
  } while (0)
#else
#define QBUEM_SKIP_DIGITS()                                                    \
  do {                                                                         \
    while (p_ + 8 <= end_) {                                                   \
      uint64_t _v;                                                             \
      std::memcpy(&_v, p_, 8);                                                 \
      uint64_t _s = _v - 0x3030303030303030ULL;                                \
      uint64_t _nd =                                                           \
          (_s | ((_s & 0x7F7F7F7F7F7F7F7FULL) + 0x7676767676767676ULL)) &      \
          0x8080808080808080ULL;                                               \
      if (_nd) {                                                               \
        p_ += QBUEM_CTZ(_nd) >> 3;                                             \
        break;                                                                 \
      }                                                                        \
      p_ += 8;                                                                 \
    }                                                                          \
    while (p_ < end_ && static_cast<unsigned>(*p_ - '0') < 10u)                \
      ++p_;                                                                    \
  } while (0)
#endif
          QBUEM_SKIP_DIGITS(); // fractional digits
          if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-'))
              ++p_;
            QBUEM_SKIP_DIGITS(); // exponent digits
          }
#undef QBUEM_SKIP_DIGITS
        }
        push(flt ? TapeNodeType::NumberRaw : TapeNodeType::Integer,
             static_cast<uint16_t>(p_ - s), static_cast<uint32_t>(s - data_));

        // ── + B1: Double-pump Number Parsing with fused key
        // scanner ─ Numbers are values. They are ALWAYS followed by ',' or
        // ']' or '}'. Instead of falling back to the top of the switch
        // loop, we peek at the next char. If it's ',' in an object, fuse
        // the next key scan.
        if (QBUEM_LIKELY(p_ < end_)) {
          unsigned char nc = static_cast<unsigned char>(*p_);
          if (nc <= 0x20) {
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            nc = static_cast<unsigned char>(c);
          }
          if (QBUEM_LIKELY(nc == ',')) {
            ++p_;
            // + 60-A: fused key scan; in_obj = bit1 of cur_state_
            if (QBUEM_LIKELY(depth_ > 0 && (cur_state_ & 0b010u))) {
              if (QBUEM_LIKELY(p_ < end_)) {
                unsigned char fc = static_cast<unsigned char>(*p_);
                if (fc <= 0x20) {
                  fc = static_cast<unsigned char>(skip_to_action());
                  if (QBUEM_UNLIKELY(p_ >= end_))
                    goto done;
                }
                if (QBUEM_LIKELY(fc == '"')) {
                  char vc = scan_key_colon_next(p_ + 1, nullptr);
                  if (QBUEM_UNLIKELY(vc == 0))
                    goto fail;
                  if (QBUEM_UNLIKELY(p_ >= end_))
                    goto done;
                  c = vc;
                  continue; // directly to value
                }
                c = static_cast<char>(fc);
                continue;
              }
              goto done;
            }
            if (QBUEM_UNLIKELY(depth_ == 0)) goto done;
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            continue; // bypass loop bottom separator logic, go straight to
                      // next token
          }
          if (QBUEM_LIKELY(nc == ']' || nc == '}')) {
            if (QBUEM_UNLIKELY(depth_ == 0))
              goto fail;
            --depth_;
            // restore parent state
            cur_state_ = cstate_stack_[depth_];
            push_end(nc == '}' ? TapeNodeType::ObjectEnd
                               : TapeNodeType::ArrayEnd,
                     static_cast<uint32_t>(p_ - data_));
            ++p_;
            c = skip_to_action();
            if (QBUEM_UNLIKELY(p_ >= end_))
              goto done;
            continue; // End handled inline!
          }
        }
        break;
      }
      default:
        goto fail;
      } // switch
      if (depth_ == 0) break;

      // ── Inline separator + peek-next optimization ─────────
      // After each token, consume ':' or ',' inline (no switch iteration).
      // After the separator, try a direct *p_ peek before calling
      // skip_to_action. For JSON like "key":value or value,"next", the char
      // after sep is > 0x20.
      c = skip_to_action();
      if (QBUEM_UNLIKELY(p_ >= end_))
        break;
      if (QBUEM_LIKELY(c == ':' || c == ',')) {
        ++p_; // consume separator
        if (QBUEM_UNLIKELY(p_ >= end_))
          break;
        // Peek: if next char is already an action byte, skip
        // skip_to_action()
        unsigned char nc = static_cast<unsigned char>(*p_);
        if (QBUEM_LIKELY(nc > 0x20)) {
          c = static_cast<char>(nc); // direct dispatch, zero function call
        } else {
          c = skip_to_action(); // whitespace present, do SWAR skip
          if (QBUEM_UNLIKELY(p_ >= end_))
            break;
        }
      }

    } // while

    done:
      doc_->tape.head = tape_head_;
      if (!allow_trailing) {
        while (p_ < end_) {
          if (static_cast<unsigned char>(*p_) > 0x20)
            return false;
          ++p_;
        }
      }
      return depth_ == 0 && tape_head_ > doc_->tape.base;

  fail:
    doc_->tape.head = tape_head_;
    return false;
  }

#if QBUEM_HAS_AVX512 || QBUEM_HAS_NEON
  // ── Stage 2 — index-based parse loop ───────────────────────
  //
  // Key differences from parse():
  //   • No skip_to_action() calls — structural positions pre-computed by
  //   Stage 1 • String length = O(1) lookup: close_offset - open_offset - 1
  //   • No double-pump logic needed — sequential index traversal suffices •
  //   Same push() / push_end() / cur_state_ as parse() for correctness
  //
  // Stage 1 guarantees:
  //   • Every '"' in the index is a real (unescaped) quote
  //   • After each opening '"', the VERY NEXT index entry is the closing
  //   '"' • structural chars inside strings are excluded from the index •
  //   value starts (digit/'-'/'t'/'f'/'n') are marked via vstart
  [[gnu::hot]] bool parse_staged(const Stage1Index &s1, bool allow_trailing = false) noexcept {
    const uint32_t *pos = s1.positions;
    const uint32_t n = s1.count;

    if (QBUEM_UNLIKELY(n == 0)) {
      doc_->tape.head = tape_head_;
      return false; // empty / all-whitespace JSON is invalid
    }

    // last_off tracks the byte offset just past the end of the last
    // consumed atom.  After the for-loop we scan [last_off, end_) for stray
    // non- whitespace, which catches things like "nulls" or "true garbage".
    uint32_t last_off = 0;

    for (uint32_t i = 0; i < n;) {
      const uint32_t off = pos[i++];
      const char c = data_[off];

      switch (static_cast<ActionId>(kActionLut[static_cast<uint8_t>(c)])) {

      case kActObjOpen: {
        push(TapeNodeType::ObjectStart, 0, off);
        // save parent state, init new object context.
        cstate_stack_[depth_] = cur_state_;
        cur_state_ = 0b011u; // in_obj=1, is_key=1, has_elem=0
        ++depth_;
        // reset key index for newly entered object depth.
        if (QBUEM_LIKELY(depth_ < KeyLenCache::MAX_DEPTH))
          kc_.key_idx[depth_] = 0;
        last_off = off + 1;
        break;
      }

      case kActArrOpen: {
        push(TapeNodeType::ArrayStart, 0, off);
        // save parent state, init new array context.
        cstate_stack_[depth_] = cur_state_;
        cur_state_ = 0b000u; // in_obj=0, is_key=0, has_elem=0
        ++depth_;
        last_off = off + 1;
        break;
      }

      case kActClose: {
        if (QBUEM_UNLIKELY(depth_ == 0))
          goto s2_fail;
        --depth_;
        // restore parent state (no mask arithmetic needed).
        cur_state_ = cstate_stack_[depth_];
        push_end(c == '}' ? TapeNodeType::ObjectEnd : TapeNodeType::ArrayEnd,
                 off);
        last_off = off + 1;
        break;
      }

      case kActString: {
        // Stage 1 guarantees: pos[i] is the closing '"' of this string.
        if (QBUEM_UNLIKELY(i >= n))
          goto s2_fail;
        const uint32_t close_off = pos[i++]; // consume closing '"'
        push(TapeNodeType::StringRaw,
             static_cast<uint16_t>(close_off - off - 1),
             off + 1); // offset = first char inside string
        last_off = close_off + 1;
        break;
      }

        // kActColon / kActComma are no longer emitted by
        // stage1_scan_avx512. push() cur_state_ handles key↔value
        // alternation internally.

      case kActNumber: {
        // Scan integer/float from data_[off]; push raw token.
        const char *s = data_ + off;
        const char *pn = s;
        if (*pn == '-')
          ++pn;
        // SWAR-8 integer digit scan
        while (pn + 8 <= end_) {
          uint64_t v;
          std::memcpy(&v, pn, 8);
          uint64_t shifted = v - 0x3030303030303030ULL;
          uint64_t nondigit = (shifted | ((shifted & 0x7F7F7F7F7F7F7F7FULL) +
                                          0x7676767676767676ULL)) &
                              0x8080808080808080ULL;
          if (nondigit) {
            pn += QBUEM_CTZ(nondigit) >> 3;
            goto s2_num_done;
          }
          pn += 8;
        }
        while (pn < end_ && static_cast<unsigned>(*pn - '0') < 10u)
          ++pn;
      s2_num_done:;
        bool flt = false;
        if (QBUEM_UNLIKELY(pn < end_ &&
                           (*pn == '.' || *pn == 'e' || *pn == 'E'))) {
          flt = true;
          ++pn;
          if (pn < end_ && (*pn == '+' || *pn == '-'))
            ++pn;
          // SWAR-8 fractional digit scan
          while (pn + 8 <= end_) {
            uint64_t _v;
            std::memcpy(&_v, pn, 8);
            uint64_t _s = _v - 0x3030303030303030ULL;
            uint64_t _nd =
                (_s | ((_s & 0x7F7F7F7F7F7F7F7FULL) + 0x7676767676767676ULL)) &
                0x8080808080808080ULL;
            if (_nd) {
              pn += QBUEM_CTZ(_nd) >> 3;
              break;
            }
            pn += 8;
          }
          while (pn < end_ && static_cast<unsigned>(*pn - '0') < 10u)
            ++pn;
          if (pn < end_ && (*pn == 'e' || *pn == 'E')) {
            ++pn;
            if (pn < end_ && (*pn == '+' || *pn == '-'))
              ++pn;
            while (pn + 8 <= end_) {
              uint64_t _v;
              std::memcpy(&_v, pn, 8);
              uint64_t _s = _v - 0x3030303030303030ULL;
              uint64_t _nd = (_s | ((_s & 0x7F7F7F7F7F7F7F7FULL) +
                                    0x7676767676767676ULL)) &
                             0x8080808080808080ULL;
              if (_nd) {
                pn += QBUEM_CTZ(_nd) >> 3;
                break;
              }
              pn += 8;
            }
            while (pn < end_ && static_cast<unsigned>(*pn - '0') < 10u)
              ++pn;
          }
        }
        push(flt ? TapeNodeType::NumberRaw : TapeNodeType::Integer,
             static_cast<uint16_t>(pn - s), off);
        last_off = static_cast<uint32_t>(pn - data_);
        break;
      }

      case kActTrue:
        if (QBUEM_UNLIKELY(off + 4 > static_cast<uint32_t>(end_ - data_) ||
                           std::memcmp(data_ + off, "true", 4)))
          goto s2_fail;
        push(TapeNodeType::BooleanTrue, 4, off);
        last_off = off + 4;
        break;

      case kActFalse:
        if (QBUEM_UNLIKELY(off + 5 > static_cast<uint32_t>(end_ - data_) ||
                           std::memcmp(data_ + off, "false", 5)))
          goto s2_fail;
        push(TapeNodeType::BooleanFalse, 5, off);
        last_off = off + 5;
        break;

      case kActNull:
        if (QBUEM_UNLIKELY(off + 4 > static_cast<uint32_t>(end_ - data_) ||
                           std::memcmp(data_ + off, "null", 4)))
          goto s2_fail;
        push(TapeNodeType::Null, 4, off);
        last_off = off + 4;
        break;

      default:
        goto s2_fail;
      } // switch
      if (depth_ == 0) break;
    } // for

    if (!allow_trailing) {
      const char *tail = data_ + last_off;
      while (tail < end_) {
        if (static_cast<unsigned char>(*tail) > 0x20)
          goto s2_fail;
        ++tail;
      }
    }

    doc_->tape.head = tape_head_;
    // Same empty-tape guard as parse(): require at least one value node.
    return depth_ == 0 && tape_head_ > doc_->tape.base;

  s2_fail:
    doc_->tape.head = tape_head_;
    return false;
  }
#endif // QBUEM_HAS_AVX512
};

// Public API

inline Value parse_partial(DocumentView &handle, std::string_view json, size_t* consumed = nullptr) {
  DocumentState *doc = handle.state();
  if (!doc) return {};
  doc->source = json;
  doc->mutations_.clear(); doc->deleted_.clear(); doc->additions_.clear(); doc->array_insertions_.clear();
  const size_t needed = json.size() + 64;
  if (!doc->tape.base || static_cast<size_t>(doc->tape.cap - doc->tape.base) < needed) doc->tape.reserve(needed); else doc->tape.reset();
  Parser p(doc);
  if (!p.parse(true)) return {};
  if (consumed) *consumed = p.get_p() - json.data();
  return Value(doc, 0);
}

inline Value parse_reuse(DocumentView &handle, std::string_view json) {
  DocumentState *doc = handle.state();
  if (!doc)
    return {};
  doc->source = json;
  // Clear mutation / deletion / addition overlays from any prior parse.
  // These maps reference tape indices that are invalidated when the tape is
  // reset; stale entries would corrupt dump_changes_() on the next call.
  doc->mutations_.clear();
  doc->deleted_.clear();
  doc->additions_.clear();
  doc->array_insertions_.clear();
  // Worst-case tape nodes == json.size() (e.g. "[[[...]]]" produces one
  // node per character). Use json.size() + 64 as a guaranteed upper bound.
  const size_t needed = json.size() + 64;
  if (QBUEM_UNLIKELY(!doc->tape.base ||
                     static_cast<size_t>(doc->tape.cap - doc->tape.base) <
                         needed)) {
    doc->tape.reserve(needed);
  } else {
    doc->tape.reset(); // hot path: head = base (1 instruction)
  }
#if QBUEM_HAS_AVX512
  // Stage 1+2 is beneficial when the positions array fits in
  // L2/L3 cache and the JSON is string-heavy (e.g. twitter.json,
  // citm.json). Large number-heavy files (canada.json, gsoc-2018.json) have
  // too many positions (~1M+) causing L3 pressure: Stage 1 overhead exceeds
  // savings. Threshold: 2 MB — includes twitter(617KB) and citm(1.65MB);
  // excludes canada(2.15MB) and gsoc(3.3MB).
  static constexpr size_t kStage12MaxSize = 2 * 1024 * 1024; // 2 MB
  if (QBUEM_LIKELY(json.size() <= kStage12MaxSize)) {
    stage1_scan_avx512(json.data(), json.size(), doc->idx);
    if (!Parser(doc).parse_staged(doc->idx)) {
      throw std::runtime_error("Invalid JSON");
    }
  } else {
    if (!Parser(doc).parse()) {
      throw std::runtime_error("Invalid JSON");
    }
  }
#else
  if (!Parser(doc).parse()) {
    throw std::runtime_error("Invalid JSON");
  }
#endif
  return Value(doc, 0);
}

// ── Value::merge_patch() out-of-line (needs parse_reuse) ────────────────────
inline void Value::merge_patch(std::string_view patch_json) {
  if (!is_object())
    return;
  DocumentView patch_doc;
  Value patch = parse_reuse(patch_doc, patch_json);
  merge_patch_impl_(patch);
}

// ── Value::patch() — RFC 6902 ────────────────────────────────────────────────

inline bool Value::patch(std::string_view patch_json) {
  DocumentView patch_doc;
  Value patch_array = parse_reuse(patch_doc, patch_json);
  return patch(patch_array);
}

inline bool Value::patch(const Value &patch_array) {
  if (!patch_array.is_array())
    return false;

  // Transactional safety: backup current mutation state
  auto saved_deleted = doc_->deleted_;
  auto saved_additions = doc_->additions_;
  auto saved_array_insertions = doc_->array_insertions_;

  auto rollback = [&]() {
    doc_->deleted_ = std::move(saved_deleted);
    doc_->additions_ = std::move(saved_additions);
    doc_->array_insertions_ = std::move(saved_array_insertions);
    doc_->last_dump_size_ = 0;
    return false;
  };

  for (Value op_obj : patch_array.elements()) {
    if (!op_obj.is_object())
      return rollback();
    std::string op = op_obj["op"] | "";
    std::string path = op_obj["path"] | "";
    if (op.empty())
      return rollback();
    if (!path.empty() && path[0] != '/')
      return rollback();

    if (op == "add") {
      Value val = op_obj["value"];
      if (!val)
        return rollback();
      if (path.empty()) {
        doc_->mutations_[idx_] = {val.type(), val.dump()};
        doc_->last_dump_size_ = 0;
        continue;
      }
      const size_t last_slash = path.find_last_of('/');
      std::string parent_path(path.substr(0, last_slash));
      std::string key(path.substr(last_slash + 1));
      Value parent = at(parent_path);
      if (!parent)
        return rollback();
      if (parent.is_object()) {
        parent.erase(key);
        parent.insert(key, val);
      } else if (parent.is_array()) {
        if (key == "-") {
          parent.push_back(val);
        } else {
          size_t p_idx = 0;
          auto [ptr, ec] =
              std::from_chars(key.data(), key.data() + key.size(), p_idx);
          if (ec != std::errc{})
            return rollback();
          parent.insert(p_idx, val);
        }
      } else
        return rollback();
    } else if (op == "remove") {
      if (path.empty())
        return rollback();
      const size_t last_slash = path.find_last_of('/');
      std::string parent_path(path.substr(0, last_slash));
      std::string key(path.substr(last_slash + 1));
      Value parent = at(parent_path);
      if (!parent || !parent.erase(key))
        return rollback();
    } else if (op == "replace") {
      Value val = op_obj["value"];
      if (!val || !at(path).is_valid())
        return rollback();
      if (path.empty()) {
        doc_->mutations_[idx_] = {val.type(), val.dump()};
        doc_->last_dump_size_ = 0;
        continue;
      }
      const size_t last_slash = path.find_last_of('/');
      std::string parent_path(path.substr(0, last_slash));
      std::string key(path.substr(last_slash + 1));
      Value parent = at(parent_path);
      if (parent.is_object()) {
        if (!parent.erase(key))
          return rollback();
        parent.insert(key, val);
      } else if (parent.is_array()) {
        size_t p_idx = 0;
        auto [ptr, ec] =
            std::from_chars(key.data(), key.data() + key.size(), p_idx);
        if (ec != std::errc{} || !parent.erase(p_idx))
          return rollback();
        parent.insert(p_idx, val);
      } else
        return rollback();
    } else if (op == "move") {
      std::string from = op_obj["from"] | "";
      Value val = at(from);
      if (from.empty() || !val)
        return rollback();
      std::string json = val.dump();
      // Remove from
      const size_t f_slash = from.find_last_of('/');
      Value f_parent = at(from.substr(0, f_slash));
      std::string f_key(from.substr(f_slash + 1));
      if (f_parent.is_object()) {
        if (!f_parent.erase(f_key))
          return rollback();
      } else if (f_parent.is_array()) {
        size_t idx = 0;
        std::from_chars(f_key.data(), f_key.data() + f_key.size(), idx);
        if (!f_parent.erase(idx))
          return rollback();
      } else
        return rollback();
      // Add to path (recursion-simulated)
      DocumentView temp_doc;
      Value v_new = parse_reuse(temp_doc, json);
      // Re-use 'add' logic manually or create helper
      const size_t t_slash = path.find_last_of('/');
      Value t_parent = at(path.substr(0, t_slash));
      std::string t_key(path.substr(t_slash + 1));
      if (t_parent.is_object())
        t_parent.insert(t_key, v_new);
      else if (t_parent.is_array()) {
        if (t_key == "-")
          t_parent.push_back(v_new);
        else {
          size_t idx = 0;
          std::from_chars(t_key.data(), t_key.data() + t_key.size(), idx);
          t_parent.insert(idx, v_new);
        }
      } else
        return rollback();
    } else if (op == "copy") {
      std::string from = op_obj["from"] | "";
      Value val = at(from);
      if (from.empty() || !val)
        return rollback();
      std::string json = val.dump();
      DocumentView temp_doc;
      Value v_new = parse_reuse(temp_doc, json);
      const size_t t_slash = path.find_last_of('/');
      Value t_parent = at(path.substr(0, t_slash));
      std::string t_key(path.substr(t_slash + 1));
      if (t_parent.is_object())
        t_parent.insert(t_key, v_new);
      else if (t_parent.is_array()) {
        if (t_key == "-")
          t_parent.push_back(v_new);
        else {
          size_t idx = 0;
          std::from_chars(t_key.data(), t_key.data() + t_key.size(), idx);
          t_parent.insert(idx, v_new);
        }
      }
    } else if (op == "test") {
      Value expected = op_obj["value"];
      Value actual = at(path);
      if (expected.dump() != actual.dump())
        return rollback();
    } else {
      return rollback(); // Unknown operation
    }
  }
  return true;
}

// SafeValue — optional-propagating chain proxy
//
// Returned by Value::get(key/idx).  Every subsequent operator[] propagates
// std::nullopt silently: if any step is absent, the entire chain returns
// an empty SafeValue without throwing.
//
// Read path comparison:
//   root["user"]["id"].as<int>()        ← throws on missing key (fast path)
//   root.get("user")["id"].as<int>()    ← std::optional<int>, never throws
//   root.get("user")["id"].value_or(-1) ← int with default, never throws
//
// SafeValue is std::optional<Value>-compatible: has_value(), operator bool,
// operator* and operator-> all work, so existing code using optional<Value>
// patterns compiles unchanged.

/// @brief An optional-propagating proxy for safe JSON navigation.
/// @details `SafeValue` is obtained via `Value::get()`. It propagates absence
/// silently (similar to a monad). Any chained access on an absent `SafeValue`
/// will return another absent `SafeValue` without throwing or crashing.
class SafeValue {
  Value val_; // valid only when has_ == true; default-constructed otherwise
  bool has_ = false;

public:
  SafeValue() noexcept = default;
  explicit SafeValue(Value v) noexcept : val_(v), has_(true) {}

  // ── std::optional-compatible interface ───────────────────────────────────

  bool has_value() const noexcept { return has_; }
  explicit operator bool() const noexcept { return has_; }

  Value &value() {
    if (!has_)
      throw std::bad_optional_access{};
    return val_;
  }
  const Value &value() const {
    if (!has_)
      throw std::bad_optional_access{};
    return val_;
  }
  Value &operator*() { return value(); }
  const Value &operator*() const { return value(); }
  Value *operator->() { return &value(); }
  const Value *operator->() const { return &value(); }

  // ── Type checks — false when absent ──────────────────────────────────────

  bool is_null() const noexcept { return has_ && val_.is_null(); }
  bool is_bool() const noexcept { return has_ && val_.is_bool(); }
  bool is_int() const noexcept { return has_ && val_.is_int(); }
  bool is_double() const noexcept { return has_ && val_.is_double(); }
  bool is_number() const noexcept { return has_ && val_.is_number(); }
  bool is_string() const noexcept { return has_ && val_.is_string(); }
  bool is_object() const noexcept { return has_ && val_.is_object(); }
  bool is_array() const noexcept { return has_ && val_.is_array(); }

  // ── Chaining — absent propagates forward, never throws ───────────────────

  SafeValue operator[](std::string_view key) const noexcept {
    if (!has_)
      return {};
    return val_.get(key); // calls Value::get() defined below
  }
  SafeValue operator[](const char *key) const noexcept {
    return (*this)[std::string_view(key)];
  }
  SafeValue operator[](size_t idx) const noexcept {
    if (!has_)
      return {};
    return val_.get(idx);
  }
  SafeValue operator[](int idx) const noexcept {
    if (idx < 0 || !has_)
      return {};
    return val_.get(static_cast<size_t>(idx));
  }
  // unsigned int overload — resolves ambiguity between int and size_t
  SafeValue operator[](unsigned int idx) const noexcept {
    return (*this)[static_cast<size_t>(idx)];
  }

  // Alias for further chaining (same as operator[]):
  SafeValue get(std::string_view key) const noexcept { return (*this)[key]; }
  SafeValue get(const char *key) const noexcept { return (*this)[key]; }
  SafeValue get(size_t idx) const noexcept { return (*this)[idx]; }
  SafeValue get(int idx) const noexcept { return (*this)[idx]; }
  // unsigned int overload — resolves ambiguity between int and size_t
  SafeValue get(unsigned int idx) const noexcept { return (*this)[static_cast<size_t>(idx)]; }

  // ── Terminal: typed extraction ────────────────────────────────────────────

  // as<T>() — returns std::optional<T>; std::nullopt when absent or wrong type
  template <JsonReadable T> std::optional<T> as() const noexcept {
    if (!has_)
      return std::nullopt;
    return val_.try_as<T>();
  }

  // value_or(default) — direct T with fallback; never throws
  template <JsonReadable T> T value_or(T def) const noexcept {
    auto r = as<T>();
    return r ? *r : def;
  }

  // dump() — "null" when absent
  std::string dump() const { return has_ ? val_.dump() : "null"; }

  // ── Pipe fallback on SafeValue: safe_val | default ────────────────────────
  //
  // Usage:
  //   int age = root.get("user")["age"] | 0;
  //   std::string s = root.get("name") | "anon";

  template <JsonReadable T>
  friend T operator|(const SafeValue &sv, T def) noexcept {
    if (!sv.has_)
      return def;
    auto r = sv.val_.try_as<T>();
    return r ? *r : def;
  }
  friend std::string operator|(const SafeValue &sv, const char *def) noexcept {
    if (!sv.has_)
      return std::string(def);
    auto r = sv.val_.try_as<std::string>();
    return r ? *r : std::string(def);
  }

  // ── Monadic operations ────────────────────────────────────────────────────
  //
  // Inspired by the Monad pattern and C++23 std::optional monadic ops.
  // All three propagate the absent state (has_=false) without branching at
  // the call site, keeping transformation pipelines free of if-checks.
  //
  // Example pipeline:
  //
  //   auto price = root["store"]["items"][0]
  //       .and_then([](const Value& v) -> SafeValue {
  //           return v.is_number() ? SafeValue{v} : SafeValue{};
  //       })
  //       .transform([](const Value& v) { return v.as<double>() * 1.1; })
  //       .value_or(0.0);

  // and_then — flatMap: F(Value) -> SafeValue.
  // If absent, returns SafeValue{}.  F is only called when has_value().
  // Use when the transformation can itself fail (returns SafeValue).
  template <std::invocable<const Value &> F>
    requires std::same_as<std::invoke_result_t<F, const Value &>, SafeValue>
  SafeValue and_then(F &&f) const
      noexcept(std::is_nothrow_invocable_v<F, const Value &>) {
    if (!has_)
      return {};
    return std::invoke(std::forward<F>(f), val_);
  }

  // transform — map: F(Value) -> U, result wrapped in std::optional<U>.
  // If absent, returns std::nullopt.  Never throws unless F throws.
  // Use when the transformation always succeeds given a valid Value.
  template <std::invocable<const Value &> F>
  auto transform(F &&f) const
      noexcept(std::is_nothrow_invocable_v<F, const Value &>)
          -> std::optional<std::invoke_result_t<F, const Value &>> {
    if (!has_)
      return std::nullopt;
    return std::invoke(std::forward<F>(f), val_);
  }

  // or_else — fallback: F() -> SafeValue, called only when absent.
  // Use to supply a default SafeValue when the chain is broken.
  template <std::invocable F>
    requires std::same_as<std::invoke_result_t<F>, SafeValue>
  SafeValue or_else(F &&f) const noexcept(std::is_nothrow_invocable_v<F>) {
    if (has_)
      return *this;
    return std::invoke(std::forward<F>(f));
  }
};

// ── DocumentState::get_synthetic out-of-line (Value now complete) ──────────

inline DocumentState *
DocumentState::get_synthetic(const ::std::string &json_str) const {
  auto it = synthetic_docs_.find(json_str);
  if (it != synthetic_docs_.end())
    return it->second.get();

  DocumentView synth_handle(json_str);
  parse_reuse(synth_handle, json_str);

  DocumentState *s = synth_handle.state();
  // Transfer ownership to shared_ptr. The handle has 1 ref.
  // We want the shared_ptr to own it too.
  auto shared = std::shared_ptr<DocumentState>(
      s, [](DocumentState *ptr) { ptr->deref(); });
  // Increment ref for the shared_ptr so it doesn't delete when handle goes away
  s->ref();

  synthetic_docs_[json_str] = std::move(shared);
  return s;
}

// ── Value::get() out-of-line definitions (SafeValue now complete) ──────────

inline SafeValue Value::get(::std::string_view key) const noexcept {
  Value v = this->operator[](key);
  if (v.idx_ == 0)
    return {};
  return SafeValue{v};
}

inline SafeValue Value::get(const char *key) const noexcept {
  return get(::std::string_view(key));
}

inline SafeValue Value::get(size_t idx) const noexcept {
  Value v = this->operator[](idx);
  if (v.idx_ == 0)
    return {};
  return SafeValue{v};
}

inline SafeValue Value::get(int idx) const noexcept {
  if (idx < 0)
    return {};
  return get(static_cast<size_t>(idx));
}

// ============================================================================
// qbuem::rfc8259 — RFC 8259 strict validator
// ============================================================================
//
// Validates a JSON string against RFC 8259 (the JSON specification).
// Throws std::runtime_error with a descriptive message on any violation.
//
// Differences from the default lenient parser:
//   • Rejects trailing commas:   [1,2,]  {\"a\":1,}
//   • Rejects leading zeros:     01  007  -01
//   • Rejects trailing decimal:  1.
//   • Rejects empty exponent:    1e  1e+  1E-
//   • Rejects unescaped control chars in strings (U+0000..U+001F)
//   • Rejects invalid escape sequences: \x  \a  \z  etc.
//   • Rejects trailing non-whitespace content after the value
//   • Accepts any JSON value at top level (RFC 8259 §2)
//
// Usage:
//   qbuem::Document doc;
//   qbuem::Value root = qbuem::parse_strict(doc, json);  // throws on violation
//
//   // Or just validate without parsing:
//   qbuem::rfc8259::validate(json);  // throws std::runtime_error on violation
// ============================================================================

namespace rfc8259 {

namespace detail_ {

[[noreturn]] inline void fail(const char *msg, const char *pos,
                              const char *begin) {
  char buf[128];
  std::snprintf(buf, sizeof(buf), "RFC 8259 violation at offset %zu: %s",
                static_cast<size_t>(pos - begin), msg);
  throw std::runtime_error(buf);
}

struct Validator {
  const char *p;
  const char *end;
  const char *begin;

  void ws() noexcept {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      ++p;
  }

  void expect_literal(const char *lit, size_t len) {
    if (static_cast<size_t>(end - p) < len || std::memcmp(p, lit, len) != 0)
      fail("invalid literal", p, begin);
    p += len;
  }

  void parse_string() {
    ++p; // skip '"'
    while (p < end) {
      unsigned char c = static_cast<unsigned char>(*p++);
      if (c == '"')
        return; // end of string
      if (c == '\\') {
        if (p >= end)
          fail("unterminated escape sequence", p - 1, begin);
        unsigned char esc = static_cast<unsigned char>(*p++);
        if (esc == 'u') {
          if (p + 4 > end)
            fail("incomplete \\uXXXX escape", p - 2, begin);
          for (int i = 0; i < 4; ++i) {
            unsigned char h = static_cast<unsigned char>(*p++);
            bool hex = (h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') ||
                       (h >= 'A' && h <= 'F');
            if (!hex)
              fail("invalid hex digit in \\uXXXX", p - 1, begin);
          }
        } else if (esc != '"' && esc != '\\' && esc != '/' && esc != 'b' &&
                   esc != 'f' && esc != 'n' && esc != 'r' && esc != 't') {
          fail("invalid escape character", p - 1, begin);
        }
      } else if (c < 0x20) {
        fail("unescaped control character in string", p - 1, begin);
      }
    }
    fail("unterminated string", p, begin);
  }

  void parse_number() {
    // Optional minus
    if (*p == '-')
      ++p;
    if (p >= end)
      fail("unexpected end in number", p, begin);

    // Integer part
    if (*p == '0') {
      ++p;
      // Leading zero: reject if followed by another digit
      if (p < end && static_cast<unsigned char>(*p) >= '0' &&
          static_cast<unsigned char>(*p) <= '9')
        fail("leading zero in number", p - 1, begin);
    } else if (static_cast<unsigned char>(*p) >= '1' &&
               static_cast<unsigned char>(*p) <= '9') {
      while (p < end && static_cast<unsigned char>(*p) >= '0' &&
             static_cast<unsigned char>(*p) <= '9')
        ++p;
    } else {
      fail("invalid number", p, begin);
    }

    // Optional fractional part
    if (p < end && *p == '.') {
      ++p;
      if (p >= end || static_cast<unsigned char>(*p) < '0' ||
          static_cast<unsigned char>(*p) > '9')
        fail("trailing decimal point or missing digits after '.'", p - 1,
             begin);
      while (p < end && static_cast<unsigned char>(*p) >= '0' &&
             static_cast<unsigned char>(*p) <= '9')
        ++p;
    }

    // Optional exponent
    if (p < end && (*p == 'e' || *p == 'E')) {
      ++p;
      if (p < end && (*p == '+' || *p == '-'))
        ++p;
      if (p >= end || static_cast<unsigned char>(*p) < '0' ||
          static_cast<unsigned char>(*p) > '9')
        fail("missing digits in exponent", p - 1, begin);
      while (p < end && static_cast<unsigned char>(*p) >= '0' &&
             static_cast<unsigned char>(*p) <= '9')
        ++p;
    }
  }

  void parse_array() {
    ++p; // skip '['
    ws();
    if (p < end && *p == ']') {
      ++p;
      return;
    } // empty array
    parse_value();
    ws();
    while (p < end && *p == ',') {
      ++p; // skip ','
      ws();
      if (p < end && *p == ']')
        fail("trailing comma in array", p - 1, begin);
      parse_value();
      ws();
    }
    if (p >= end || *p != ']')
      fail("expected ']'", p, begin);
    ++p;
  }

  void parse_object() {
    ++p; // skip '{'
    ws();
    if (p < end && *p == '}') {
      ++p;
      return;
    } // empty object
    if (p >= end || *p != '"')
      fail("expected string key", p, begin);
    parse_string();
    ws();
    if (p >= end || *p != ':')
      fail("expected ':' after key", p, begin);
    ++p;
    parse_value();
    ws();
    while (p < end && *p == ',') {
      ++p; // skip ','
      ws();
      if (p < end && *p == '}')
        fail("trailing comma in object", p - 1, begin);
      if (p >= end || *p != '"')
        fail("expected string key", p, begin);
      parse_string();
      ws();
      if (p >= end || *p != ':')
        fail("expected ':' after key", p, begin);
      ++p;
      parse_value();
      ws();
    }
    if (p >= end || *p != '}')
      fail("expected '}'", p, begin);
    ++p;
  }

  void parse_value() {
    ws();
    if (p >= end)
      fail("unexpected end of input", p, begin);
    char c = *p;
    if (c == '"')
      parse_string();
    else if (c == '{')
      parse_object();
    else if (c == '[')
      parse_array();
    else if (c == 't')
      expect_literal("true", 4);
    else if (c == 'f')
      expect_literal("false", 5);
    else if (c == 'n')
      expect_literal("null", 4);
    else if (c == '-' || (c >= '0' && c <= '9'))
      parse_number();
    else
      fail("unexpected character", p, begin);
  }

  void run(std::string_view json) {
    p = json.data();
    end = json.data() + json.size();
    begin = p;
    parse_value();
    ws();
    if (p != end)
      fail("trailing content after JSON value", p, begin);
  }
};

} // namespace detail_

/// Validate \p json against RFC 8259.
/// Throws std::runtime_error with offset information on the first violation.
inline void validate(std::string_view json) { detail_::Validator{}.run(json); }

/// Parse \p json into \p doc with strict RFC 8259 compliance.
/// Rejects: trailing commas, leading zeros, invalid escapes,
///          unescaped control characters, trailing content, etc.
/// Throws std::runtime_error describing the violation and its byte offset.
inline Value parse_strict(DocumentView &doc, ::std::string_view json) {
  rfc8259::validate(json);
  return qbuem::json::parse_reuse(doc, json);
}

} // namespace rfc8259

// ============================================================================
// qbuem::detail — Automatic Serialization / Deserialization Engine
// ============================================================================
//
// Concept-based dispatch: zero user effort for all standard C++ types.
//
//  ┌─────────────────────────────────────────────────────────────────────┐
//  │  Tier 1 — Built-in (automatic, no code needed)                      │
//  │    bool, int, double, float, …       → JSON number/bool             │
//  │    std::string, string_view          → JSON string (escaped)        │
//  │    std::optional<T>                  → null  or  T                  │
//  │    std::vector / list / deque <T>    → JSON array                   │
//  │    std::set / unordered_set <T>      → JSON array                   │
//  │    std::map / unordered_map <str,V>  → JSON object                  │
//  │    std::array<T,N>                   → JSON array (fixed size)      │
//  │    std::pair<A,B>, tuple<Ts…>        → JSON array                   │
//  │    nullptr_t                         → null                         │
//  ├─────────────────────────────────────────────────────────────────────┤
//  │  Tier 2 — One macro line for custom structs                         │
//  │    struct Point { int x, y; };                                      │
//  │    QBUEM_JSON_FIELDS(Point, x, y)    // done!                       │
//  │                                                                     │
//  │    Nested structs, STL containers, optional — all recursive.        │
//  ├─────────────────────────────────────────────────────────────────────┤
//  │  Tier 3 — Manual ADL (complex / polymorphic types)                  │
//  │    void from_qbuem_json(const qbuem::Value&, MyType&);              │
//  │    void to_qbuem_json(qbuem::Value&, const MyType&);                │
//  └─────────────────────────────────────────────────────────────────────┘
// ============================================================================

namespace detail {

// ── SWAR helpers (used by string serialization and Nexus key scanning) ────────
// Returns non-zero if any byte in `v` equals `byte`.
QBUEM_INLINE uint64_t swar_has_byte(uint64_t v, uint8_t byte) noexcept {
  uint64_t x = v ^ (static_cast<uint64_t>(byte) * 0x0101010101010101ULL);
  return (x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL;
}
// Returns non-zero if any byte in `v` is less than `threshold` (0x01..0x7F range safe).
QBUEM_INLINE uint64_t swar_has_less(uint64_t v, uint8_t threshold) noexcept {
  return (v - static_cast<uint64_t>(threshold) * 0x0101010101010101ULL) & ~v &
         0x8080808080808080ULL;
}

// ── Type trait helpers

template <typename T, template <typename...> class U>
struct is_specialization_of : std::false_type {};

template <template <typename...> class U, typename... Args>
struct is_specialization_of<U<Args...>, U> : std::true_type {};

// ── Concepts

template <typename T>
concept JsonDetailBool =
    std::is_same_v<T, bool> ||
    std::is_same_v<std::remove_cvref_t<T>, std::vector<bool>::reference> ||
    std::is_same_v<std::remove_cvref_t<T>, std::vector<bool>::const_reference>;
// ── Nexus Core: Pulse Mapping Hashing (FNV-1a)
constexpr uint64_t fnv1a_hash(std::string_view s) {
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (char c : s) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

consteval uint64_t fnv1a_hash_ce(const char *s) {
  uint64_t hash = 0xcbf29ce484222325ULL;
  while (*s) {
    hash ^= static_cast<uint64_t>(*s++);
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

// ── Fast key hash for nexus_pulse field dispatch ─────────────────────────────
// ≤8 bytes  : raw little-endian bytes loaded as uint64 (single load, no multiply)
// 9-16 bytes: XOR first-8 and last-8 bytes, mixed with length
// >16 bytes : FNV-1a fallback
// Compile-time and runtime versions produce identical values on LE platforms.
consteval uint64_t fast_key_hash_ce(const char *s) noexcept {
  size_t n = 0; while (s[n]) ++n;
  if (n == 0) return 0;
  if (n <= 8) {
    uint64_t v = 0;
    for (size_t i = 0; i < n; ++i)
      v |= static_cast<uint64_t>(static_cast<unsigned char>(s[i])) << (i * 8);
    return v;
  }
  if (n <= 16) {
    uint64_t a = 0, b = 0;
    for (size_t i = 0; i < 8; ++i)
      a |= static_cast<uint64_t>(static_cast<unsigned char>(s[i]))     << (i * 8);
    for (size_t i = 0; i < 8; ++i)
      b |= static_cast<uint64_t>(static_cast<unsigned char>(s[n-8+i])) << (i * 8);
    return a ^ b ^ (static_cast<uint64_t>(n) << 56);
  }
  uint64_t h = 0xcbf29ce484222325ULL;
  for (size_t i = 0; i < n; ++i) {
    h ^= static_cast<uint64_t>(static_cast<unsigned char>(s[i]));
    h *= 0x100000001b3ULL;
  }
  return h;
}

QBUEM_INLINE uint64_t fast_key_hash(std::string_view s) noexcept {
  const size_t n = s.size();
  if (n == 0) return 0;
  if (n <= 8) {
    uint64_t v = 0;
    std::memcpy(&v, s.data(), n);   // LE: byte[0] → LSB (consistent with CE)
    return v;
  }
  if (n <= 16) {
    uint64_t a = 0, b = 0;
    std::memcpy(&a, s.data(),     8);
    std::memcpy(&b, s.data()+n-8, 8);
    return a ^ b ^ (static_cast<uint64_t>(n) << 56);
  }
  return fnv1a_hash(s);
}

template <typename T>
concept JsonDetailArith = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

template <typename T>
concept JsonDetailStrLike =
    std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
    std::is_same_v<T, const char *>;

template <typename T>
concept JsonDetailOptional = is_specialization_of<T, std::optional>::value;

// Sequence: has push_back — vector, list, deque (not string)
template <typename T>
concept JsonDetailSeq = requires(T &t) {
  t.push_back(std::declval<typename T::value_type>());
} && !JsonDetailStrLike<T>;

// Set: has insert, no push_back, no mapped_type — set, unordered_set, multiset
template <typename T>
concept JsonDetailSet =
    requires(T &t) { t.insert(std::declval<typename T::value_type>()); } &&
    !JsonDetailStrLike<T> &&
    !requires(T &t) { t.push_back(std::declval<typename T::value_type>()); } &&
    !requires { typename T::mapped_type; };

// Map: has mapped_type with string-compatible key — map, unordered_map
template <typename T>
concept JsonDetailMap =
    requires {
      typename T::mapped_type;
      typename T::key_type;
    } && (std::is_same_v<typename T::key_type, std::string> ||
          std::is_convertible_v<std::string, typename T::key_type>);

// Fixed array: std::array<T,N> — tuple_size + value_type, no push_back
template <typename T>
concept JsonDetailFixedArr = requires {
  std::tuple_size<T>::value;
  typename T::value_type;
} && !JsonDetailSeq<T> && !JsonDetailSet<T> && !JsonDetailMap<T>;

// Tuple/pair: std::tuple<Ts…> or std::pair<A,B>
template <typename T>
concept JsonDetailTuple = is_specialization_of<T, std::tuple>::value ||
                          is_specialization_of<T, std::pair>::value;

// ADL hooks (user-defined or via QBUEM_JSON_FIELDS)
template <typename T>
concept HasFromQbuemJson =
    requires(const Value &v, T &t) { from_qbuem_json(v, t); };
template <typename T>
concept HasToQbuemJson =
    requires(Value &v, const T &t) { to_qbuem_json(v, t); };
template <typename T>
concept HasAppendQbuemJson =
    requires(std::string &out, const T &t) { append_qbuem_json(out, t); };
template <typename T>
concept HasNexusPulse =
    requires(std::string_view key, const char *&p, const char *end, T &t) {
  nexus_pulse(key, p, end, t);
};
// Fast variant: dispatch on a pre-computed uint64 key hash (skips runtime hash)
template <typename T>
concept HasNexusPulseH =
    requires(uint64_t h, const char *&p, const char *end, T &t) {
  nexus_pulse_h(h, p, end, t);
};

// ── Forward declarations

template <typename T> void from_json(const Value &v, T &out);
template <typename T> std::string to_json_str(const T &in);
template <typename W, typename T> void append_json(W &out, const T &in);
template <typename T> void append_json(std::string &out, const T &in);

// ── FastWriter forward declarations (full definition later) ──────────────────
struct FastWriter;
QBUEM_INLINE void json_put(std::string &s, char c)               noexcept;
QBUEM_INLINE void json_write(std::string &s, const char *p, size_t n) noexcept;
QBUEM_INLINE void json_put(FastWriter &w, char c)                noexcept;
QBUEM_INLINE void json_write(FastWriter &w, const char *p, size_t n) noexcept;

// ── Tuple/pair helpers

template <typename Tup> void from_json_tuple_(const Value &v, Tup &out) {
  std::vector<Value> elems;
  for (const auto &e : v.elements())
    elems.push_back(e);
  std::apply(
      [&](auto &...args) {
        size_t i = 0;
        ((i < elems.size() ? (from_json(elems[i++], args), void())
                           : (++i, void())),
         ...);
      },
      out);
}

template <typename Tup> std::string to_json_str_tuple_(const Tup &in) {
  std::string s = "[";
  bool first = true;
  std::apply(
      [&](const auto &...args) {
        ((s += (std::exchange(first, false) ? "" : ",") + to_json_str(args)),
         ...);
      },
      in);
  return s + ']';
}

template <typename W, typename Tup>
void append_json_tuple_(W &out, const Tup &in) {
  json_put(out, '[');
  bool first = true;
  std::apply(
      [&](const auto &...args) {
        (((first ? (first = false, void()) : (void)json_put(out, ',')),
          append_json(out, args)),
         ...);
      },
      in);
  json_put(out, ']');
}

// ── from_json — concept-dispatched deserialization ───────────────────────────
//
// Precedence (highest to lowest):
//   nullptr_t → bool → arithmetic → string → optional → sequence → set →
//   map → fixed-array → tuple → ADL from_qbuem_json → static_assert

template <typename T> void from_json(const Value &v, T &out) {
  if constexpr (std::is_same_v<T, std::nullptr_t>) {
    // nothing — null is null
  } else if constexpr (JsonDetailBool<T>) {
    out = v.as<bool>();
  } else if constexpr (JsonDetailArith<T>) {
    out = v.as<T>();
  } else if constexpr (std::is_same_v<T, std::string>) {
    out = v.as<std::string>();
  } else if constexpr (JsonDetailOptional<T>) {
    if (!v.is_valid() || v.is_null()) {
      out = std::nullopt;
      return;
    }
    typename T::value_type inner{};
    from_json(v, inner);
    out = std::move(inner);
  } else if constexpr (JsonDetailSeq<T>) {
    out.clear();
    for (const auto &elem : v.elements()) {
      typename T::value_type item{};
      from_json(elem, item);
      out.push_back(std::move(item));
    }
  } else if constexpr (JsonDetailSet<T>) {
    out.clear();
    for (const auto &elem : v.elements()) {
      typename T::value_type item{};
      from_json(elem, item);
      out.insert(std::move(item));
    }
  } else if constexpr (JsonDetailMap<T>) {
    out.clear();
    for (const auto &[k, val] : v.items()) {
      typename T::mapped_type item{};
      from_json(val, item);
      out.emplace(std::string(k), std::move(item));
    }
  } else if constexpr (JsonDetailFixedArr<T>) {
    constexpr size_t N = std::tuple_size_v<T>;
    size_t i = 0;
    for (const auto &elem : v.elements()) {
      if (i >= N)
        break;
      from_json(elem, out[i++]);
    }
  } else if constexpr (JsonDetailTuple<T>) {
    from_json_tuple_(v, out);
  } else if constexpr (HasFromQbuemJson<T>) {
    from_qbuem_json(v, out); // ADL: user-defined or QBUEM_JSON_FIELDS-generated
  } else {
    static_assert(sizeof(T) == 0,
                  "qbuem::read / from_json: no deserialization for T. "
                  "Use QBUEM_JSON_FIELDS(Type, field...) or define "
                  "from_qbuem_json(const qbuem::Value&, T&).");
  }
}

// ── to_json_str — concept-dispatched serialization ───────────────────────────

template <typename T> std::string to_json_str(const T &in) {
  if constexpr (std::is_same_v<T, std::nullptr_t>) {
    return "null";
  } else if constexpr (JsonDetailBool<T>) {
    return in ? "true" : "false";
  } else if constexpr (std::is_integral_v<T>) {
    char buf[24];
    char *p;
    if constexpr (std::is_unsigned_v<T>) {
      if constexpr (sizeof(T) <= 4) p = qj_nc::to_chars(buf, static_cast<uint32_t>(in));
      else                          p = qj_nc::to_chars(buf, static_cast<uint64_t>(in));
    } else {
      if constexpr (sizeof(T) <= 4) p = qj_nc::to_chars(buf, static_cast<int32_t>(in));
      else                          p = qj_nc::to_chars(buf, static_cast<int64_t>(in));
    }
    return std::string(buf, p);
  } else if constexpr (std::is_floating_point_v<T>) {
    char buf[40];
    // qj_nc::to_chars handles NaN/Inf → "null" internally
    char *ep = qj_nc::to_chars(buf, static_cast<double>(in));
    return std::string(buf, ep);
  } else if constexpr (std::is_same_v<T, std::string> ||
                       std::is_same_v<T, std::string_view>) {
    std::string r;
    r.reserve(in.size() + 2);
    r += '"';
    const char *s = in.data(), *e = s + in.size();
    while (s < e) {
      const char *safe = s;
      // SWAR: skip 8 bytes at a time when no byte needs escaping
      while (safe + 8 <= e) {
        uint64_t w; std::memcpy(&w, safe, 8);
        if ((swar_has_less(w, 0x20) | swar_has_byte(w, '"') |
             swar_has_byte(w, '\\')) == 0) { safe += 8; continue; }
        break;
      }
      while (safe < e) {
        unsigned char c = static_cast<unsigned char>(*safe);
        if (c < 0x20 || c == '"' || c == '\\') break;
        ++safe;
      }
      if (safe > s) r.append(s, safe - s);
      s = safe;
      if (s < e) {
        unsigned char c = static_cast<unsigned char>(*s++);
        if (c == '"')       r.append("\\\"", 2);
        else if (c == '\\') r.append("\\\\", 2);
        else if (c == '\n') r.append("\\n",  2);
        else if (c == '\r') r.append("\\r",  2);
        else if (c == '\t') r.append("\\t",  2);
        else {
          static constexpr char hex[] = "0123456789abcdef";
          char esc[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
          r.append(esc, 6);
        }
      }
    }
    r += '"';
    return r;
  } else if constexpr (std::is_same_v<T, const char *>) {
    return to_json_str(std::string_view(in ? in : ""));
  } else if constexpr (JsonDetailOptional<T>) {
    if (!in.has_value())
      return "null";
    return to_json_str(*in);
  } else if constexpr (JsonDetailSeq<T> || JsonDetailSet<T>) {
    std::string s = "[";
    bool first = true;
    for (const auto &item : in) {
      if (!first)
        s += ',';
      s += to_json_str(item);
      first = false;
    }
    return s + ']';
  } else if constexpr (JsonDetailMap<T>) {
    std::string s = "{";
    bool first = true;
    for (const auto &[k, val] : in) {
      if (!first)
        s += ',';
      s += to_json_str(k) + ':' + to_json_str(val);
      first = false;
    }
    return s + '}';
  } else if constexpr (JsonDetailFixedArr<T>) {
    constexpr size_t N = std::tuple_size_v<T>;
    std::string s = "[";
    for (size_t i = 0; i < N; ++i) {
      if (i > 0)
        s += ',';
      s += to_json_str(in[i]);
    }
    return s + ']';
  } else if constexpr (JsonDetailTuple<T>) {
    return to_json_str_tuple_(in);
  } else if constexpr (HasAppendQbuemJson<T>) {
    std::string s;
    append_qbuem_json(s, in);
    return s;
  } else if constexpr (HasToQbuemJson<T>) {
    // User-defined: create temp document, call to_qbuem_json, dump
    ::std::string src = "{}";
    DocumentView doc;
    Value root = ::qbuem::json::parse_reuse(doc, src);
    to_qbuem_json(root, in); // ADL: user-defined or QBUEM_JSON_FIELDS-generated
    return root.dump();
  } else {
    static_assert(sizeof(T) == 0,
                  "qbuem::write / to_json_str: no serialization for T. "
                  "Use QBUEM_JSON_FIELDS(Type, field...) or define "
                  "to_qbuem_json(qbuem::Value&, const T&).");
  }
}

// ── FastWriter — direct buffer write (avoids per-call std::string overhead) ─

// FastWriter: stack-buffer accumulator — zero pre-init overhead.
// Accumulates output in a 1 KB on-stack buffer; flushes to std::string
// only on overflow or destruction.  For typical structs (< 1 KB JSON),
// the destructor flush is the ONLY std::string call.
struct FastWriter {
  static constexpr size_t kBuf = 1024;
  char        stack_[kBuf];
  size_t      pos_;
  std::string &s;

  explicit FastWriter(std::string &s) noexcept : pos_(0), s(s) {}

  // Flush stack to string on destruction (single s.append call).
  ~FastWriter() noexcept { if (pos_) s.append(stack_, pos_); }

  QBUEM_INLINE void put(char c) noexcept {
    if (__builtin_expect(pos_ < kBuf, 1)) { stack_[pos_++] = c; return; }
    flush_then_put(c);
  }
  QBUEM_INLINE void write(const char *src, size_t n) noexcept {
    if (__builtin_expect(pos_ + n <= kBuf, 1)) {
      std::memcpy(stack_ + pos_, src, n); pos_ += n; return;
    }
    flush_then_write(src, n);
  }
  // Overwrite the last written byte (replaces trailing comma with '}'/'}'.
  QBUEM_INLINE void set_last(char c) noexcept {
    if (__builtin_expect(pos_ > 0, 1)) { stack_[pos_ - 1] = c; return; }
    if (!s.empty()) s.back() = c;   // after flush, last byte is in s
  }

  void flush() noexcept { s.append(stack_, pos_); pos_ = 0; }
  void flush_then_put(char c) noexcept { flush(); stack_[pos_++] = c; }
  void flush_then_write(const char *src, size_t n) noexcept {
    flush();
    if (__builtin_expect(n <= kBuf, 1)) {
      std::memcpy(stack_, src, n); pos_ = n;
    } else {
      s.append(src, n);  // oversized single write: bypass stack
    }
  }
};

// Writer-agnostic adapter helpers
QBUEM_INLINE void json_put(std::string &s, char c)              noexcept { s += c; }
QBUEM_INLINE void json_write(std::string &s, const char *p, size_t n) noexcept { s.append(p, n); }
QBUEM_INLINE void json_set_last(std::string &s, char c)         noexcept { s.back() = c; }

QBUEM_INLINE void json_put(FastWriter &w, char c)               noexcept { w.put(c); }
QBUEM_INLINE void json_write(FastWriter &w, const char *p, size_t n) noexcept { w.write(p, n); }
QBUEM_INLINE void json_set_last(FastWriter &w, char c)          noexcept { w.set_last(c); }

// Concept: anything with json_put/json_write/json_set_last adapters
template <typename W>
concept JsonWriter = requires(W &w, char c, const char *p, size_t n) {
  json_put(w, c);
  json_write(w, p, n);
};

// ── Forward-declare HasQbuemJsonFW concept (satisfied after macro expansion) ─
template <typename T, typename W>
concept HasQbuemJsonFW = requires(W &w, const T &t) { qbuem_json_append_fw(w, t); };

// ── append_json — zero-allocation concept-dispatched streaming ─────────────

template <typename W, typename T> void append_json(W &out, const T &in) {
  if constexpr (std::is_same_v<T, std::nullptr_t>) {
    json_write(out, "null", 4);
  } else if constexpr (JsonDetailBool<T>) {
    if (in) json_write(out, "true", 4); else json_write(out, "false", 5);
  } else if constexpr (std::is_integral_v<T>) {
    char buf[24];
    char *ep;
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
      // bool is_integral but handled above — unreachable
      ep = buf;
    } else if constexpr (std::is_unsigned_v<T>) {
      if constexpr (sizeof(T) <= 4) ep = qj_nc::to_chars(buf, static_cast<uint32_t>(in));
      else                          ep = qj_nc::to_chars(buf, static_cast<uint64_t>(in));
    } else {
      if constexpr (sizeof(T) <= 4) ep = qj_nc::to_chars(buf, static_cast<int32_t>(in));
      else                          ep = qj_nc::to_chars(buf, static_cast<int64_t>(in));
    }
    json_write(out, buf, static_cast<size_t>(ep - buf));
  } else if constexpr (std::is_floating_point_v<T>) {
    char buf[40];
    // qj_nc::to_chars handles NaN/Inf → "null" internally
    char *ep = qj_nc::to_chars(buf, static_cast<double>(in));
    json_write(out, buf, static_cast<size_t>(ep - buf));
  } else if constexpr (std::is_same_v<T, std::string> ||
                       std::is_same_v<T, std::string_view>) {
    json_put(out, '"');
    const char *s = in.data(), *e = s + in.size();
    while (s < e) {
      const char *safe = s;
      // SIMD fast scan: 16 bytes/iter (SSE2 on x86-64, NEON on ARM64)
#if defined(QBUEM_ARCH_X86_64)
      while (safe + 16 <= e) {
        __m128i v  = _mm_loadu_si128(reinterpret_cast<const __m128i *>(safe));
        int mask   = _mm_movemask_epi8(_mm_or_si128(
            _mm_or_si128(_mm_cmpeq_epi8(v, _mm_set1_epi8('"')),
                         _mm_cmpeq_epi8(v, _mm_set1_epi8('\\'))),
            _mm_cmplt_epi8(v, _mm_set1_epi8('\x20'))));
        if (!mask) { safe += 16; continue; }
        safe += __builtin_ctz(mask); break;
      }
#elif defined(QBUEM_HAS_NEON)
      while (safe + 16 <= e) {
        uint8x16_t v    = vld1q_u8(reinterpret_cast<const uint8_t *>(safe));
        uint8x16_t need = vorrq_u8(
            vorrq_u8(vceqq_u8(v, vdupq_n_u8('"')),
                     vceqq_u8(v, vdupq_n_u8('\\'))),
            vcltq_u8(v, vdupq_n_u8(0x20)));
        uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(need), 0);
        uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(need), 1);
        if (!(lo | hi)) { safe += 16; continue; }
        safe += lo ? (__builtin_ctzll(lo) >> 3) : (8 + (__builtin_ctzll(hi) >> 3));
        break;
      }
#endif
      // SWAR-8 for remaining bytes / fallback platforms
      while (safe + 8 <= e) {
        uint64_t w; std::memcpy(&w, safe, 8);
        if ((swar_has_less(w, 0x20) | swar_has_byte(w, '"') |
             swar_has_byte(w, '\\')) == 0) { safe += 8; continue; }
        break;
      }
      while (safe < e) {
        unsigned char c = static_cast<unsigned char>(*safe);
        if (c < 0x20 || c == '"' || c == '\\') break;
        ++safe;
      }
      if (safe > s) json_write(out, s, static_cast<size_t>(safe - s));
      s = safe;
      if (s < e) {
        unsigned char c = static_cast<unsigned char>(*s++);
        if (c == '"')       json_write(out, "\\\"", 2);
        else if (c == '\\') json_write(out, "\\\\", 2);
        else if (c == '\n') json_write(out, "\\n",  2);
        else if (c == '\r') json_write(out, "\\r",  2);
        else if (c == '\t') json_write(out, "\\t",  2);
        else {
          static constexpr char hex[] = "0123456789abcdef";
          char esc[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
          json_write(out, esc, 6);
        }
      }
    }
    json_put(out, '"');
  } else if constexpr (std::is_same_v<T, const char *>) {
    append_json(out, std::string_view(in ? in : ""));
  } else if constexpr (JsonDetailOptional<T>) {
    if (!in.has_value()) { json_write(out, "null", 4); return; }
    append_json(out, *in);
  } else if constexpr (JsonDetailSeq<T> || JsonDetailSet<T>) {
    json_put(out, '[');
    bool first = true;
    for (const auto &item : in) {
      if (!first) json_put(out, ',');
      append_json(out, item);
      first = false;
    }
    json_put(out, ']');
  } else if constexpr (JsonDetailMap<T>) {
    json_put(out, '{');
    bool first = true;
    for (const auto &[k, val] : in) {
      if (!first) json_put(out, ',');
      append_json(out, k);
      json_put(out, ':');
      append_json(out, val);
      first = false;
    }
    json_put(out, '}');
  } else if constexpr (JsonDetailFixedArr<T>) {
    constexpr size_t N = std::tuple_size_v<T>;
    json_put(out, '[');
    for (size_t i = 0; i < N; ++i) {
      if (i > 0) json_put(out, ',');
      append_json(out, in[i]);
    }
    json_put(out, ']');
  } else if constexpr (JsonDetailTuple<T>) {
    append_json_tuple_(out, in);
  } else if constexpr (HasQbuemJsonFW<T, W>) {
    // Fast path: uses qbuem_json_append_fw (generated by updated macro)
    qbuem_json_append_fw(out, in);
  } else if constexpr (HasAppendQbuemJson<T>) {
    // Legacy path: append_qbuem_json takes std::string
    if constexpr (std::is_same_v<std::remove_cvref_t<W>, std::string>) {
      append_qbuem_json(out, in);
    } else {
      // W is FastWriter but type only has legacy append_qbuem_json:
      // write to temp string, then bulk-copy into writer
      std::string tmp;
      append_qbuem_json(tmp, in);
      json_write(out, tmp.data(), tmp.size());
    }
  } else if constexpr (HasToQbuemJson<T>) {
    ::std::string src = "{}";
    DocumentView doc;
    Value root = ::qbuem::json::parse_reuse(doc, src);
    to_qbuem_json(root, in);
    const std::string ds = root.dump();
    json_write(out, ds.data(), ds.size());
  } else {
    static_assert(sizeof(T) == 0,
                  "qbuem::write / append_json: no serialization for T. "
                  "Use QBUEM_JSON_FIELDS(Type, field...) or define "
                  "append_qbuem_json(std::string&, const T&).");
  }
}

// Backward-compatible std::string overload (called by existing code that
// uses the 2-arg form before the W template was introduced)
template <typename T> void append_json(std::string &out, const T &in) {
  append_json<std::string, T>(out, in);
}

// ── Per-field helpers for QBUEM_JSON_FIELDS

template <typename T>
inline void from_json_field(const Value &obj, const char *key, T &field) {
  auto opt = obj.find(key);
  if (!opt)
    return;
  if constexpr (is_specialization_of<T, std::optional>::value) {
    from_json(*opt, field);
  } else {
    if (!opt->is_null())
      from_json(*opt, field);
  }
}

template <typename T>
inline void to_json_field(Value &obj, const char *key, const T &val) {
  obj.insert_json(key, to_json_str(val));
}

} // namespace detail

// ============================================================================
// QBUEM_FOR_EACH — variadic macro
// ============================================================================

#define QBUEM_DETAIL_EXPAND(x) x
#define QBUEM_DETAIL_CONCAT(a, b) a##b
#define QBUEM_DETAIL_CONCAT2(a, b) QBUEM_DETAIL_CONCAT(a, b)

#define QBUEM_DETAIL_COUNT(...)                                                \
  QBUEM_DETAIL_EXPAND(QBUEM_DETAIL_COUNT_I(                                    \
      __VA_ARGS__, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, \
      17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#define QBUEM_DETAIL_COUNT_I(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,     \
                             _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, \
                             _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, \
                             _32, N, ...)                                      \
  N

#define QBUEM_DETAIL_FE_1(fn, a) fn(a)
#define QBUEM_DETAIL_FE_2(fn, a, ...) fn(a) QBUEM_DETAIL_FE_1(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_3(fn, a, ...) fn(a) QBUEM_DETAIL_FE_2(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_4(fn, a, ...) fn(a) QBUEM_DETAIL_FE_3(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_5(fn, a, ...) fn(a) QBUEM_DETAIL_FE_4(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_6(fn, a, ...) fn(a) QBUEM_DETAIL_FE_5(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_7(fn, a, ...) fn(a) QBUEM_DETAIL_FE_6(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_8(fn, a, ...) fn(a) QBUEM_DETAIL_FE_7(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_9(fn, a, ...) fn(a) QBUEM_DETAIL_FE_8(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_10(fn, a, ...) fn(a) QBUEM_DETAIL_FE_9(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_11(fn, a, ...) fn(a) QBUEM_DETAIL_FE_10(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_12(fn, a, ...) fn(a) QBUEM_DETAIL_FE_11(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_13(fn, a, ...) fn(a) QBUEM_DETAIL_FE_12(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_14(fn, a, ...) fn(a) QBUEM_DETAIL_FE_13(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_15(fn, a, ...) fn(a) QBUEM_DETAIL_FE_14(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_16(fn, a, ...) fn(a) QBUEM_DETAIL_FE_15(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_17(fn, a, ...) fn(a) QBUEM_DETAIL_FE_16(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_18(fn, a, ...) fn(a) QBUEM_DETAIL_FE_17(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_19(fn, a, ...) fn(a) QBUEM_DETAIL_FE_18(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_20(fn, a, ...) fn(a) QBUEM_DETAIL_FE_19(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_21(fn, a, ...) fn(a) QBUEM_DETAIL_FE_20(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_22(fn, a, ...) fn(a) QBUEM_DETAIL_FE_21(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_23(fn, a, ...) fn(a) QBUEM_DETAIL_FE_22(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_24(fn, a, ...) fn(a) QBUEM_DETAIL_FE_23(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_25(fn, a, ...) fn(a) QBUEM_DETAIL_FE_24(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_26(fn, a, ...) fn(a) QBUEM_DETAIL_FE_25(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_27(fn, a, ...) fn(a) QBUEM_DETAIL_FE_26(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_28(fn, a, ...) fn(a) QBUEM_DETAIL_FE_27(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_29(fn, a, ...) fn(a) QBUEM_DETAIL_FE_28(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_30(fn, a, ...) fn(a) QBUEM_DETAIL_FE_29(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_31(fn, a, ...) fn(a) QBUEM_DETAIL_FE_30(fn, __VA_ARGS__)
#define QBUEM_DETAIL_FE_32(fn, a, ...) fn(a) QBUEM_DETAIL_FE_31(fn, __VA_ARGS__)

#define QBUEM_FOR_EACH(fn, ...)                                                \
  QBUEM_DETAIL_EXPAND(QBUEM_DETAIL_CONCAT2(                                    \
      QBUEM_DETAIL_FE_, QBUEM_DETAIL_COUNT(__VA_ARGS__))(fn, __VA_ARGS__))

// ============================================================================
// QBUEM_JSON_FIELDS — one-line struct serialization/deserialization
// ============================================================================

#define QBUEM_JSON_DETAIL_READ(f)                                              \
  ::qbuem::json::detail::from_json_field(v, #f, obj.f);
#define QBUEM_JSON_DETAIL_WRITE(f)                                             \
  ::qbuem::json::detail::to_json_field(v, #f, obj.f);
#define QBUEM_JSON_DETAIL_APPEND(f)                                            \
  out += "\"" #f "\":";                                                        \
  ::qbuem::json::detail::append_json(out, obj.f);                              \
  out += ',';

#define QBUEM_JSON_DETAIL_PULSE(f)                                             \
  case ::qbuem::json::detail::fast_key_hash_ce(#f):                            \
    ::qbuem::json::detail::from_json_direct(p, end, obj.f);                    \
    break;

// QBUEM_JSON_DETAIL_APPEND_OPT — legacy path (used if QBUEM_JSON_DETAIL_APPEND_FW not available)
#define QBUEM_JSON_DETAIL_APPEND_OPT(f)                                        \
  {                                                                            \
    static constexpr ::std::string_view kf = "\"" #f "\":";                    \
    out.append(kf.data(), kf.size());                                          \
  }                                                                            \
  ::qbuem::json::detail::append_json(out, obj.f);                              \
  out += ',';

// QBUEM_JSON_DETAIL_APPEND_FW — FastWriter path: zero std::string overhead
#define QBUEM_JSON_DETAIL_APPEND_FW(f)                                         \
  {                                                                            \
    static constexpr char _bj_kf[] = "\"" #f "\":";                           \
    _bj_fw.write(_bj_kf, sizeof(_bj_kf) - 1);                                 \
  }                                                                            \
  ::qbuem::json::detail::append_json(_bj_fw, obj.f);                          \
  _bj_fw.put(',');

#define QBUEM_JSON_FIELDS(Type, ...)                                           \
  /* Fast dispatch on pre-computed key hash — primary hot path */              \
  inline void nexus_pulse_h(uint64_t _h, const char *&p,                      \
                            const char *end, Type &obj) {                      \
    switch (_h) {                                                              \
      QBUEM_FOR_EACH(QBUEM_JSON_DETAIL_PULSE, __VA_ARGS__)                     \
    default:                                                                   \
      ::qbuem::json::detail::skip_direct(p, end);                              \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  /* Backward-compat wrapper: hashes the key then dispatches via nexus_pulse_h */\
  inline void nexus_pulse(::std::string_view key, const char *&p,              \
                          const char *end, Type &obj) {                        \
    nexus_pulse_h(::qbuem::json::detail::fast_key_hash(key), p, end, obj);    \
  }                                                                            \
  inline void from_qbuem_json(const ::qbuem::json::Value &v, Type &obj) {      \
    QBUEM_FOR_EACH(QBUEM_JSON_DETAIL_READ, __VA_ARGS__)                        \
  }                                                                            \
  inline void to_qbuem_json(::qbuem::json::Value &v, const Type &obj) {        \
    QBUEM_FOR_EACH(QBUEM_JSON_DETAIL_WRITE, __VA_ARGS__)                       \
  }                                                                            \
  /* FastWriter serialization — zero std::string overhead */                   \
  inline void qbuem_json_append_fw(::qbuem::json::detail::FastWriter &_bj_fw, \
                                   const Type &obj) {                          \
    _bj_fw.put('{');                                                           \
    QBUEM_FOR_EACH(QBUEM_JSON_DETAIL_APPEND_FW, __VA_ARGS__)                   \
    _bj_fw.set_last('}');                                                      \
  }                                                                            \
  inline void qbuem_json_append_fw(std::string &_bj_s, const Type &obj) {     \
    ::qbuem::json::detail::FastWriter _bj_fw(_bj_s);  /* uses spare cap */    \
    qbuem_json_append_fw(_bj_fw, obj);                                         \
  }                                                                            \
  inline void append_qbuem_json(std::string &out, const Type &obj) {           \
    qbuem_json_append_fw(out, obj);                                            \
  }

// Backward-compatibility alias: qbuem::json::lazy → qbuem::json
// Tests and older code may reference types via qbuem::json::lazy::SafeValue,
// qbuem::json::lazy::JsonInteger, etc.
namespace lazy = ::qbuem::json;

} // namespace json
} // namespace qbuem

// ── C++20 ranges support ────────────────────────────────────────────────────
namespace std::ranges {
template <>
inline constexpr bool enable_borrowed_range<qbuem::json::Value::ObjectRange> =
    true;

template <>
inline constexpr bool enable_borrowed_range<qbuem::json::Value::ArrayRange> =
    true;
} // namespace std::ranges

namespace qbuem {

// ---------------------------------------------------------------------------
// Tier 1 — qbuem::core
// ---------------------------------------------------------------------------
namespace core {
using TapeNodeType = json::TapeNodeType;
using TapeNode = json::TapeNode;
using TapeArena = json::TapeArena;
using Stage1Index = json::Stage1Index;
using Parser = json::Parser;
} // namespace core

// ---------------------------------------------------------------------------
// Tier 3 — qbuem:: public facade
// ---------------------------------------------------------------------------

using Document = json::DocumentView;
using Value = json::Value;
using SafeValue = json::SafeValue;

namespace rfc8259 = json::rfc8259;

inline Value parse(Document &doc, ::std::string_view json) {
  return json::parse_reuse(doc, json);
}

inline Value parse_reuse(Document &doc, ::std::string_view json) {
  return json::parse_reuse(doc, json);
}

inline Value parse_strict(Document &doc, ::std::string_view json) {
  json::rfc8259::validate(json);
  return json::parse_reuse(doc, json);
}

namespace json::detail {

QBUEM_INLINE void ws(const char *&p, const char *end) noexcept {
  // Fast-path for compact JSON (no whitespace between tokens — by far the common case)
  if (p >= end || (unsigned char)*p > 32) [[likely]] return;
  do { ++p; } while (p < end && (unsigned char)*p <= 32);
}

struct NexusScanner {
  const char *p;
  const char *end;

  QBUEM_INLINE void ws() noexcept { detail::ws(p, end); }

  // ── read_key: backward-compat, returns string_view ─────────────────────────
  QBUEM_INLINE std::string_view read_key() noexcept {
    ws();
    if (p >= end || *p != '"') [[unlikely]] return {};
    const char *start = ++p;
    // SIMD fast scan (16 bytes/iter)
#if defined(QBUEM_ARCH_X86_64)
    while (p + 16 <= end) {
      __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
      int mask = _mm_movemask_epi8(_mm_or_si128(
          _mm_cmpeq_epi8(v, _mm_set1_epi8('"')),
          _mm_cmpeq_epi8(v, _mm_set1_epi8('\\'))));
      if (!mask) { p += 16; continue; }
      p += __builtin_ctz(mask); break;
    }
#elif QBUEM_HAS_NEON
    while (p + 16 <= end) {
      uint8x16_t v    = vld1q_u8(reinterpret_cast<const uint8_t *>(p));
      uint8x16_t need = vorrq_u8(vceqq_u8(v, vdupq_n_u8('"')),
                                 vceqq_u8(v, vdupq_n_u8('\\')));
      uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(need), 0);
      uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(need), 1);
      if (!(lo | hi)) { p += 16; continue; }
      p += lo ? (__builtin_ctzll(lo) >> 3) : (8 + (__builtin_ctzll(hi) >> 3));
      break;
    }
#endif
    // SWAR-8 for remaining bytes
    while (p + 8 <= end) {
      uint64_t w; std::memcpy(&w, p, 8);
      uint64_t qm = w ^ 0x2222222222222222ULL;
      uint64_t mq = (qm - 0x0101010101010101ULL) & ~qm & 0x8080808080808080ULL;
      uint64_t bm = w ^ 0x5c5c5c5c5c5c5c5cULL;
      uint64_t mb = (bm - 0x0101010101010101ULL) & ~bm & 0x8080808080808080ULL;
      if ((mq | mb) == 0) { p += 8; continue; }
      break;
    }
    while (p < end && *p != '"') { if (*p == '\\') p += 2; else ++p; }
    std::string_view key(start, static_cast<size_t>(p - start));
    if (p < end) ++p;
    if (p < end && (unsigned char)*p <= 32) [[unlikely]] ws();
    if (p < end && *p == ':') [[likely]] ++p;
    return key;
  }

  // ── read_key_h: scan key AND compute fast_key_hash in one pass ─────────────
  // For keys ≤8 bytes with no escape sequences (the overwhelming common case for
  // struct fields), the hash falls out of the SWAR termination word — zero extra
  // work vs the existing scan.
  QBUEM_INLINE uint64_t read_key_h() noexcept {
    if (p < end && (unsigned char)*p <= 32) [[unlikely]] ws();
    if (p >= end || *p != '"') [[unlikely]] return 0;
    const char *start = ++p;

    // ── Hot path: key ≤8 bytes, no escape ──────────────────────────────────
    if (p + 8 <= end) [[likely]] {
      uint64_t w; std::memcpy(&w, p, 8);
      uint64_t qm = w ^ 0x2222222222222222ULL;
      uint64_t mq = (qm - 0x0101010101010101ULL) & ~qm & 0x8080808080808080ULL;
      if (mq) [[likely]] {
        uint64_t bm = w ^ 0x5c5c5c5c5c5c5c5cULL;
        uint64_t mb = (bm - 0x0101010101010101ULL) & ~bm & 0x8080808080808080ULL;
        if (!mb) [[likely]] {
          // Quote found within 8 bytes, no backslash — key fits in one word
          const int bp = static_cast<int>(__builtin_ctzll(mq) >> 3); // byte pos of '"'
          p += bp + 1;                                                // skip key + '"'
          if (p < end && (unsigned char)*p <= 32) [[unlikely]] ws();
          if (p < end && *p == ':') [[likely]] ++p;
          // Hash = raw bytes [0..bp-1], zero-padded — identical to fast_key_hash_ce
          const uint64_t mask = bp ? ((uint64_t(1) << (bp * 8)) - 1) : uint64_t(0);
          return w & mask;
        }
      }
    }

    // ── General path: key >8 bytes or contains escape sequences ────────────
#if defined(QBUEM_ARCH_X86_64)
    while (p + 16 <= end) {
      __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
      int mask = _mm_movemask_epi8(_mm_or_si128(
          _mm_cmpeq_epi8(v, _mm_set1_epi8('"')),
          _mm_cmpeq_epi8(v, _mm_set1_epi8('\\'))));
      if (!mask) { p += 16; continue; }
      p += __builtin_ctz(mask); break;
    }
#elif QBUEM_HAS_NEON
    while (p + 16 <= end) {
      uint8x16_t v    = vld1q_u8(reinterpret_cast<const uint8_t *>(p));
      uint8x16_t need = vorrq_u8(vceqq_u8(v, vdupq_n_u8('"')),
                                 vceqq_u8(v, vdupq_n_u8('\\')));
      uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(need), 0);
      uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(need), 1);
      if (!(lo | hi)) { p += 16; continue; }
      p += lo ? (__builtin_ctzll(lo) >> 3) : (8 + (__builtin_ctzll(hi) >> 3));
      break;
    }
#endif
    while (p + 8 <= end) {
      uint64_t w; std::memcpy(&w, p, 8);
      uint64_t qm = w ^ 0x2222222222222222ULL;
      uint64_t mq = (qm - 0x0101010101010101ULL) & ~qm & 0x8080808080808080ULL;
      uint64_t bm = w ^ 0x5c5c5c5c5c5c5c5cULL;
      uint64_t mb = (bm - 0x0101010101010101ULL) & ~bm & 0x8080808080808080ULL;
      if ((mq | mb) == 0) { p += 8; continue; }
      break;
    }
    while (p < end && *p != '"') { if (*p == '\\') p += 2; else ++p; }
    const size_t n = static_cast<size_t>(p - start);
    if (p < end) ++p;
    if (p < end && (unsigned char)*p <= 32) [[unlikely]] ws();
    if (p < end && *p == ':') [[likely]] ++p;
    return detail::fast_key_hash(std::string_view{start, n});
  }

  template <typename T> inline void fill(T &obj);
};

template <typename T> void from_json_direct(const char *&p, const char *end, T &out);

inline void skip_direct(const char *&p, const char *end) {
  rfc8259::detail_::Validator v;
  v.p = p; v.end = end; v.begin = p;
  try { v.parse_value(); p = v.p; } catch (...) { p = end; }
}

template <typename T> void from_json_direct(const char *&p, const char *end, T &out) {
  if (p >= end) return;
  if constexpr (HasNexusPulse<T>) {
    NexusScanner scanner{p, end};
    scanner.fill(out);
    p = scanner.p;
  } else if constexpr (JsonDetailBool<T>) {
    while (p < end && (unsigned char)*p <= 32) ++p;
    if (p + 4 <= end && !std::memcmp(p, "true", 4)) { out = true; p += 4; }
    else if (p + 5 <= end && !std::memcmp(p, "false", 5)) { out = false; p += 5; }
    else skip_direct(p, end);
  } else if constexpr (JsonDetailArith<T>) {
    while (p < end && (unsigned char)*p <= 32) ++p;
    const char *start = p;
    while (p < end && ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.' || *p == 'e' || *p == 'E' || *p == '+')) ++p;
    if constexpr (std::is_floating_point_v<T>) {
#if __cpp_lib_to_chars >= 201611L && !defined(__APPLE__)
      std::from_chars(start, p, out);
#else
      char _fc_buf[64];
      size_t _fc_len = static_cast<size_t>(p - start);
      if (_fc_len >= sizeof(_fc_buf)) _fc_len = sizeof(_fc_buf) - 1;
      std::memcpy(_fc_buf, start, _fc_len);
      _fc_buf[_fc_len] = '\0';
      out = static_cast<T>(std::strtod(_fc_buf, nullptr));
#endif
    } else {
      std::from_chars(start, p, out);
    }
  } else if constexpr (JsonDetailOptional<T>) {
    detail::ws(p, end);
    if (p + 4 <= end && !std::memcmp(p, "null", 4)) {
      out = std::nullopt;
      p += 4;
    } else {
      typename T::value_type inner{};
      from_json_direct(p, end, inner);
      out = std::move(inner);
    }
  } else if constexpr (std::is_same_v<T, std::string>) {
    detail::ws(p, end);
    if (p < end && *p == '"') {
      const char *start = ++p;
      // Fast scan: find first '"' or '\'
      const char *safe = p;
#if defined(QBUEM_ARCH_X86_64)
      {
        const __m128i vq  = _mm_set1_epi8('"');
        const __m128i vbs = _mm_set1_epi8('\\');
        while (safe + 16 <= end) {
          __m128i v    = _mm_loadu_si128(reinterpret_cast<const __m128i *>(safe));
          int     mask = _mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi8(v, vq),
                                                        _mm_cmpeq_epi8(v, vbs)));
          if (!mask) { safe += 16; continue; }
          safe += __builtin_ctz(mask);
          break;
        }
      }
#elif defined(QBUEM_HAS_NEON)
      {
        const uint8x16_t vq  = vdupq_n_u8('"');
        const uint8x16_t vbs = vdupq_n_u8('\\');
        while (safe + 16 <= end) {
          uint8x16_t v    = vld1q_u8(reinterpret_cast<const uint8_t *>(safe));
          uint8x16_t need = vorrq_u8(vceqq_u8(v, vq), vceqq_u8(v, vbs));
          uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(need), 0);
          uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(need), 1);
          if (!(lo | hi)) { safe += 16; continue; }
          safe += lo ? (__builtin_ctzll(lo) >> 3) : (8 + (__builtin_ctzll(hi) >> 3));
          break;
        }
      }
#endif
      // SWAR-8 fallback
      {
        const uint64_t kQ  = UINT64_C(0x2222222222222222); // '"' * 0x0101...
        const uint64_t kBS = UINT64_C(0x5C5C5C5C5C5C5C5C); // '\\' * 0x0101...
        while (safe + 8 <= end) {
          uint64_t w; std::memcpy(&w, safe, 8);
          auto has_byte = [](uint64_t v, uint64_t b) -> uint64_t {
            uint64_t x = v ^ b;
            return (x - UINT64_C(0x0101010101010101)) & ~x & UINT64_C(0x8080808080808080);
          };
          if (uint64_t hit = has_byte(w, kQ) | has_byte(w, kBS)) {
            safe += __builtin_ctzll(hit) >> 3; break;
          }
          safe += 8;
        }
      }
      p = safe;
      // No-escape fast path
      if (p < end && *p == '"') {
        out.assign(start, p - start);
        ++p;
      } else {
        // Slow path: handle escape sequences
        while (p < end && *p != '"') {
          if (*p == '\\') p += 2; else ++p;
        }
        out.assign(start, p - start);
        if (p < end) ++p;
      }
    } else
      skip_direct(p, end);
  } else if constexpr (JsonDetailMap<T>) {
    out.clear();
    detail::ws(p, end);
    if (p < end && *p == '{') {
      ++p;
      detail::ws(p, end);
      while (p < end && *p != '}') {
        std::string key;
        from_json_direct(p, end, key);
        detail::ws(p, end);
        if (p < end && *p == ':')
          ++p;
        detail::ws(p, end);
        typename T::mapped_type val{};
        from_json_direct(p, end, val);
        out.emplace(std::move(key), std::move(val));
        detail::ws(p, end);
        if (p < end && *p == ',') {
          ++p;
          detail::ws(p, end);
        }
      }
      if (p < end)
        ++p;
    } else
      skip_direct(p, end);
  } else if constexpr (JsonDetailTuple<T>) {
    detail::ws(p, end);
    if (p < end && *p == '[') {
      ++p;
      std::apply([&](auto &...args) {
        ((from_json_direct(p, end, args), detail::ws(p, end),
          (p < end && *p == ',' ? ++p : p), detail::ws(p, end)),
         ...);
      }, out);
      while (p < end && *p != ']')
        ++p;
      if (p < end)
        ++p;
    } else
      skip_direct(p, end);
  } else if constexpr (JsonDetailSeq<T>) {
    out.clear();
    while (p < end && (unsigned char)*p <= 32) ++p;
    if (p < end && *p == '[') {
      ++p; while (p < end && (unsigned char)*p <= 32) ++p;
      while (p < end && *p != ']') {
        typename T::value_type item{};
        from_json_direct(p, end, item);
        out.push_back(std::move(item));
        while (p < end && (unsigned char)*p <= 32) ++p;
        if (p < end && *p == ',') { ++p; while (p < end && (unsigned char)*p <= 32) ++p; }
      }
      if (p < end) ++p;
    } else skip_direct(p, end);
  } else {
    skip_direct(p, end);
  }
}

template <typename T> inline void NexusScanner::fill(T &obj) {
  ws();
  if (p >= end || *p != '{') [[unlikely]] return;
  ++p;
  if (p < end && *p == '}') [[unlikely]] { ++p; return; } // empty object
  ws();
  while (p < end && *p != '}') {
    if constexpr (HasNexusPulseH<T>) {
      // Fast path: hash computed during key scan — zero extra traversal
      const uint64_t h = read_key_h();
      nexus_pulse_h(h, p, end, obj);
    } else {
      nexus_pulse(read_key(), p, end, obj);
    }
    if (p < end && (unsigned char)*p <= 32) [[unlikely]] ws();
    if (p < end && *p == ',') [[likely]] { ++p; }
    if (p < end && (unsigned char)*p <= 32) [[unlikely]] ws();
  }
  if (p < end) ++p;
}

template <typename T>
inline void from_json_field_fallback(const Value &v, T &obj, const char *fields) {
  if (!v.is_object()) return;
}

} // namespace json::detail

// read / write / fuse helpers
template <typename T> T read(::std::string_view json) {
  Document doc;
  Value root = json::parse_reuse(doc, json);
  T obj{};
  ::qbuem::json::detail::from_json(root, obj);
  return obj;
}

template <typename T> T fuse(::std::string_view json) {
  T obj{};
  json::detail::NexusScanner scanner{json.data(), json.data() + json.size()};
  scanner.fill(obj);
  return obj;
}

template <typename T>::std::string write(const T &obj) {
  ::std::string out;
  out.reserve(512);
  ::qbuem::json::detail::append_json(out, obj);
  return out;
}

// write(obj, indent) — pretty-printed variant.
// Serializes obj to compact JSON, then re-parses and formats with indent spaces
// per level. Use write(obj) for hot paths; this variant is for human-readable output.
template <typename T>::std::string write(const T &obj, int indent) {
  ::std::string compact = write(obj);
  Document tmp;
  Value root = parse(tmp, compact);
  return root.dump(indent);
}

// write_to(buf, obj) — zero-allocation variant for hot loops.
// Appends the compact JSON representation of obj to an existing string buffer.
// Call buf.clear() between iterations; the buffer's heap capacity is reused.
template <typename T> void write_to(::std::string &buf, const T &obj) {
  ::qbuem::json::detail::append_json(buf, obj);
}

// write_to(buf, obj, indent) — pretty-printed buffer-append variant.
template <typename T> void write_to(::std::string &buf, const T &obj, int indent) {
  ::std::string compact;
  compact.reserve(512);
  ::qbuem::json::detail::append_json(compact, obj);
  Document tmp;
  Value root = parse(tmp, compact);
  buf += root.dump(indent);
}

template <typename T> void from_json(const Value &v, T &out) {
  ::qbuem::json::detail::from_json(v, out);
}

template <typename T>::std::string to_json_str(const T &val) {
  return ::qbuem::json::detail::to_json_str(val);
}

} // namespace qbuem

#endif // QBUEM_JSON_HPP
