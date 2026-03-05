# Architecture Optimization Failure Log

> **마지막 업데이트**: 2026-03-05 (M1 Phase 72-81 회귀 사례 추가 — serialize LTO/I-cache 황금 규칙 완성)
> 이 문서는 Beast JSON 라이브러리 개발 중 각 아키텍처(x86_64, aarch64 등)별로 시도되었으나 실패(성능 회귀, Regression)한 최적화 사례와 그 원인을 면밀히 분석한 자료입니다.
> 에이전트는 새로운 SIMD 최적화를 시도하기 전에 **반드시 이 문서를 숙지**하여 동일한 실수를 반복하지 않도록 해야 합니다.

---

## 1. aarch64 (NEON) 최적화 회귀 사례

### ❌ [Phase 49 시도] NEON 64B (4×16B) 구조적 스캐너 및 공백 스킵 처리
* **목표**: x86_64의 AVX-512 64B 청크 단위 스캐닝(`_mm512_loadu_si512` 등) 성능 개선을 벤치마킹하여, NEON에서도 16바이트(`uint8x16_t`) 4개를 한 번에 언롤링(Unrolling)하여 64B 단위로 처리.
* **적용 대상**: `skip_to_action()`, `scan_string_end()`, `scan_key_colon_next()`
* **실패 결과 (성능 대폭 하락)**: 
  * `twitter.json`: 269μs -> 360μs (+33% 회귀)
  * `citm_catalog.json`: 639μs -> 1037μs (+62% 회귀)
  * `gsoc-2018.json`: 627μs -> 885μs (+41% 회귀)
* **근본 원인 분석**:
  1. **명령어 집합의 근본적 차이**: AVX-512는 64바이트를 '단 하나의 명령어'(`__m512i`)로 로드/비교/비트마스크 변환이 가능합니다. 하지만 NEON에는 64바이트 단일 벡터 레지스터가 존재하지 않습니다. 
  2. **레지스터 스필(Register Spill) 및 중첩 연산 과부하**: 로드(`vld1q_u8`) × 4번, 비교(`vcgtq_u8`, `vceqq_u8`) × 4번 이상, 합산(`vorrq_u8`) 연속 3번 이상이 핫 루프에 추가되면서 컴파일러의 레지스터 할당 한계를 초과하여 CPU 파이프라인이 심각하게 스톨(Stall)되었습니다.
  3. **비트마스크 병목**: AVX는 `_mm512_cmpgt_epi8_mask` 하나로 64비트 정수(mask)를 바로 뽑아내어 `__builtin_ctzll`로 분기가 빠릅니다. 반면 NEON에는 기본 `movemask` 명령이 없어 `vmaxvq_u32`로 1차 판별 후 `neon_movemask()` 유틸리티 함수(시프트, 곱 연산, 리덕션 트리 사용)를 추가 호출해야 하므로 4개의 16B 마스크를 OR로 모아서 처리하는 과정의 연산 비용이 루프를 4번 도는 비용보다 훨씬 컸습니다.
  4. **진입 비용(Startup Overhead) 극대화**: `twitter.json` 등은 문자열이나 공백의 런(run) 스팬이 매우 짧습니다. 64B 크기를 뭉텅이로 처리하기 전 진입하는 셋업/조건 비용 자체가 커서, 오히려 16바이트로 빠르게 스킵하고 즉각 탈출하는 것이 aarch64 아키텍처에서 가장 효율적인 파레토 최적 지점이었습니다.
* **가이드라인 (에이전트 수칙)**:
  * **AArch64 에이전트**: NEON에서 32바이트 이상을 루프 언롤링하여 억지로 처리하려 하지 마세요. NEON 환경은 `uint8x16_t`를 1~2개 처리하는 수준에서 분기를 최소화하는 루프가 최적입니다. x86_64의 AVX-512 극단적 기법을 NEON에 1:1로 이식하려 시도하지 마세요.

### ❌ [Phase 50 시도] NEON Stage 1 구조적 문자 사전 인덱싱 (Two-Phase Parsing)
* **목표**: AVX-512에서 45% (365μs → 202μs) 파싱 속도 향상을 입증한 `stage1_scan_avx512`를 모방하여, NEON에서도 `stage1_scan_neon`을 구현하고 `parse_staged`로 데이터 처리 구조 변경. 64B 단위로 `vld1q_u8` 4회 언롤링 스코프 사용.
* **적용 대상**: `beast_json.hpp` 내 `stage1_scan_neon` 및 `parse_reuse` 라우팅 로직
* **실패 결과 (성능 회귀)**: 
  * `twitter.json`: 328μs → 478μs (**+45% 회귀**)
  * `citm_catalog.json`: ~645μs → 933μs (**+44% 회귀**)
* **근본 원인 분석**:
  1. **Neon_movemask의 과도한 오버헤드**: AVX-512에서는 `_mm512_cmp*_mask` 명령 하나로 64비트 정수 마스크가 바로 도출됩니다. 반면 NEON에서는 16바이트 청크마다 `vshrq_n_u8`, `vmulq_u8`, `vpaddlq_*` 등의 리덕션 트리를 타고 내려와야 하는 고비용의 소프트웨어 에뮬레이션(`neon_movemask`)이 필요합니다.
  2. **1루프 당 20개 이상의 movemask 호출 비용 발생**: 루프 1번당 4번의 16B 청크를 처리하고, 청크 하나당 Quote, Backslash, Brackets, Separators, Whitespace의 5개 마스크를 생성해야 하므로 총 20개의 `neon_movemask` 호출이 64B를 스캔할 때마다 발생합니다.
  3. **비효율적 아키텍처 매핑**: AArch64 (M1 등) 프로세서는 파이프라인과 비순차 실행(Out-of-order execution) 및 분기 예측(Branch Prediction) 성능이 매우 뛰어납니다. 64B 단위로 무거운 산술 연산을 욱여넣는 "SIMD 내 인덱싱" 기법보다, 짧은 바이트를 빠르게 읽고 루프로 바로 건너뛰는 기존 "Single Pass 선형 파서"가 더 오버헤드 없이 잘 동작합니다.
* **가이드라인 (에이전트 수칙)**:
  * **AArch64 에이전트**: SIMD "마스킹" 기반의 Two-Phase 처리 (미리 구조적 문자의 인덱스를 뽑고 나중에 파싱하는 simdjson 류의 방식)는 ARM64 환경에 적합하지 않습니다. NEON에는 Native MoveMask가 없다는 점을 항상 기억하고, 단일 경로 선형 파서 최적화에 집중하세요.

### ❌ [Phase 50-1 시도] NEON Single-Pass 스캐너 32B 언롤링 및 Branchless Pinpoint
* **목표**: `skip_to_action()`과 `scan_string_end()`의 NEON 루프에서 16B 단위 처리를 32B 단위(2× `vld1q_u8`)로 언롤링하고, 발견 시 `while`문을 통한 스칼라 스캔이 아닌 `vgetq_lane_u64(_m, 0)` 추출 및 `CTZ`를 사용해 브랜치리스(Branchless)하게 오프셋을 계산.
* **적용 대상**: `beast_json.hpp` 내 `skip_to_action()`, `scan_string_end()` 의 `#elif BEAST_HAS_NEON` 조건부 컴파일 블록.
* **실패 결과 (성능 회귀)**: 
  * `twitter.json`: 328μs → 357μs (**+8.8% 회귀**)
  * `citm_catalog.json`: 645μs → 839μs (**+30% 회귀**)
* **근본 원인 분석**:
  1. **AArch64의 브랜치 친화성**: x86_64 환경에서 극단적으로 회피해야 할 `while` 루프(스칼라 바이트 비교)가, 오히려 AArch64에서는 거대한 패널티 없이 아주 효율적으로 동작했습니다.
  2. **NEON Lane Extraction(vgetq_lane)의 고비용**: NEON 레지스터(`uint8x16_t` / `uint64x2_t`)에서 일반 범용 레지스터(GPR)로 `vgetq_lane_u64`를 사용해 데이터를 넘기는 과정(Cross-register file transfer)은 지연 속도(Latency)가 높습니다. `vmaxvq_u32`로 히트를 감지한 뒤 `vgetq_lane` 두 번과 `CTZ`를 호출하는 브랜치리스 코드가, 차라리 바이트 포인터 연산을 1바이트씩 N번 루프 도는 것보다 현격하게 느렸습니다.
  3. **과도한 루프 언롤링**: 32B 언롤링은 16개 이상의 레지스터를 여유 있게 사용할 때 좋지만, 문자열 스캐닝 특성 상 16B 만에 바로 탈출조건이 만족되는 경우가 대다수입니다. 필요 없이 16B를 추가 로드(`load` / `ceq` / `orr`) 하는 작업이 오히려 16B 이내 탈출 패턴인 일반 JSON 구조를 지연시켰습니다.
* **가이드라인 (에이전트 수칙)**:
  * **AArch64 에이전트**: NEON 스캐너 구현 시 '히트(조건 만족)' 감지 이후 위치를 좁혀야 할 때, `vgetq_lane`을 이용해 마스크를 빼내어 `CTZ`를 돌리지 마세요. 차라리 스칼라 `while` 루프로 해당 청크(16B) 내부를 바이트 단위로 찾는 것이 AArch64에서는 압도적으로 빠릅니다. 또한 핫 패스의 문자열/공백 찾기는 16B(`vld1q_u8` 1회) 단위로 가장 기민하게 처리하는 것이 최고 효율을 냅니다.
* **✅ 극복 사례 (Phase 50-2)**: GPR 기반의 SWAR-8 스킵 로직을 완전히 제거하고, SIMD 히트 시 단순 `while (*p != '"' && *p != '\\') ++p;` 스칼라 루프로 즉각 폴백하도록 구조를 롤백·단순화시켰습니다. 그 결과 `twitter.json` 파싱이 328μs에서 **253μs** (-23%)로 압도적인 성능 향상을 보이며 단일 경로 스캐너의 궁극적인 최적화 형태를 찾았습니다.

---

## 2. x86_64 (AVX2 / AVX-512) 최적화 회귀 사례

### ❌ [Phase 37] AVX2 공백 스킵 스캐닝 (Whitespace Skip)
* **목표**: `skip_to_action()` 함수에서 SWAR-32/SWAR-8 대신 `__m256i` 기반 AVX2 최적화를 도입하여 공백 무시 속도를 극대화.
* **실패 결과**: 전체적으로 **+13% 파싱 시간 증가 (성능 하락)**.
* **근본 원인 분석**:
  * 공백 길이에 대한 JSON 데이터의 '분포'를 간과했습니다. 대부분의 JSON 파일(특히 `twitter.json`)에서 토큰 사이의 공백은 고작 1~8바이트 수준에 불과합니다.
  * AVX2 레지스터에 값을 로딩(set)하고 `_mm256_movemask_epi8` 결과를 뽑아내어 `ctz`를 수행하는 진입 지연(Setup/Teardown Overhead)이, 고작 평균 2~3바이트의 공백을 스킵하는 이득보다 수 배 이상 컸습니다.
* **해결법 (Phase 46)**: AVX-512 64B 스킵퍼 도입 시, **반드시 직전에 SWAR-8(8바이트 체크) Pre-gate를 배치**하여, 8바이트 미만의 공백은 SIMD 레지스터를 깨우지 않고 스칼라단에서 즉시 처리 후 탈출하게 만들었습니다. 

### ❌ [Phase 40] AVX2 벡터 상수 호이스팅 (Constant Hoisting)
* **목표**: SIMD 스캐너 내부에서 매 루프 선언하는 상수 벡터(`_mm256_set1_epi8('"')` 등)를 루프 바깥 혹은 클래스 멤버로 호이스팅(끌어올림)하여 생성 오버헤드를 줄이려는 시도.
* **실패 결과**: 의도와 정반대로 **+10~14% 성능 회귀** 발생.
* **근본 원인 분석**:
  * 현대 x86 컴파일러(GCC, Clang)는 SIMD 함수 내 상수 선언을 보면, 레지스터 할당 스크래치패드를 분석해 완벽한 위치에 `vbroadcast` 시키거나 `.rodata` 참조로 최적화해냅니다.
  * 프로그래머가 이를 억지로 루프 바깥으로 끄집어내면 레지스터 압력이 증가(Register Pressure)하여 오히려 컴파일러가 해당 레지스터 값을 스택에 Spill/Reload 하는 최악의 코드를 생성합니다.
* **가이드라인 (에이전트 수칙)**:
  * **x86_64 에이전트**: SIMD 상수는 **항상 사용 지점(스코프)에 가장 가깝게(인접 선언)** `const __m256i v = _mm256_set1...` 형태로 선언할 것. 컴파일러의 레지스터 프로모션을 절대 방해하지 마세요. (AVX-512도 동일)

---

### ❌ [Phase 49] 브랜치리스 push() 비트스택 연산 (Branchless Bit-Stack in push())
* **목표**: `push()` 함수 내 `bool` 타입 + 삼항 연산자(CMOV) 패턴을, 순수 정수 산술(NEG+AND)로 대체하여 분기를 완전 제거.
  ```cpp
  // 기존 (컴파일러가 CMOV 생성)
  const bool in_obj = !!(obj_bits_ & mask);
  sep = is_val ? uint8_t(2) : uint8_t(has_el);
  kv_key_bits_ ^= (in_obj ? mask : uint64_t(0));

  // 시도 (NEG+AND 방식)
  const uint64_t in_obj = (obj_bits_ & mask) != 0;
  sep = static_cast<uint8_t>((is_val << 1) | (~is_val & has_el));
  kv_key_bits_ ^= (-in_obj) & mask;
  ```
* **적용 대상**: `push()` 함수 (line ~5863)
* **실패 결과 (성능 회귀)**:
  * `twitter.json`: 365μs → 370μs (+1.4%)
  * `citm_catalog.json`: 955μs → 992μs (+3.9%)
  * `gsoc-2018.json`: 751μs → 770μs (+2.5%)
  * `canada.json`: 1,416μs → 1,497μs (+5.7%) ← 최대 회귀
* **근본 원인 분석**:
  1. **CMOV은 이미 최적**: GCC/Clang `-O3`는 `bool + 삼항` 패턴에서 단일 `cmov` 명령을 생성합니다. 이것이 곧 branchless 코드이므로, 명시적 정수 산술로 바꿔도 이점이 없습니다.
  2. **명령어 수 증가**: `(is_val << 1) | (~is_val & has_el)` 는 4개 명령(SHL, NOT, AND, OR)을 생성합니다. 컴파일러의 CMOV는 단 1개 명령(`cmovne`)입니다.
  3. **NEG+AND vs CMOV**: `(-in_obj) & mask`는 NEG+AND+XOR = 3 ops, 원래 `(in_obj ? mask : 0) ^= kv_key_bits_`는 CMOV+XOR = 2 ops로 오히려 더 효율적이었습니다.
* **가이드라인 (에이전트 수칙)**:
  * **x86_64 에이전트**: `bool` + 삼항 연산자 패턴을 NEG+AND 방식의 `uint64_t` 정수 산술로 대체하지 마세요. 컴파일러는 이미 최적의 CMOV를 생성하며, 명시적 정수 산술은 명령어 수를 오히려 늘립니다. 특히 `push()` 같은 hotpath 함수에서 컴파일러의 최적화를 간섭하지 마세요.

---

### ❌ [Phase 51] 64비트 TapeNode 단일 스토어 (Single 64-bit Store)
* **목표**: `push()` / `push_end()` 에서 두 개의 32비트 필드 스토어를 단일 `uint64_t` packed 스토어(`__builtin_memcpy`)로 통합하여 store 횟수를 절반으로 줄이려는 시도.
  ```cpp
  // 기존 (32비트 ×2)
  n->meta   = (uint32_t(t) << 24) | (uint32_t(sep) << 16) | uint32_t(l);
  n->offset = o;

  // 시도 (64비트 ×1)
  const uint64_t packed = static_cast<uint64_t>(meta_val) | (static_cast<uint64_t>(o) << 32);
  TapeNode *n = tape_head_++;
  __builtin_memcpy(n, &packed, 8);
  ```
* **적용 대상**: `push()` (line ~5895), `push_end()` (line ~5910)
* **실패 결과 (심각한 성능 회귀)**:
  * `twitter.json`: 365μs → 408μs (**+11.7%**)
  * `citm_catalog.json`: 955μs → 1,093μs (**+14.4%**)
  * `gsoc-2018.json`: 751μs → 811μs (+8.0%)
  * `canada.json`: 1,416μs → 1,502μs (+6.1%)
* **근본 원인 분석**:
  1. **컴파일러 스토어 병합(Store Merging) 방해**: GCC/Clang `-O3`는 인접한 두 32비트 스토어를 자동으로 단일 64비트 스토어(`movq`)로 병합합니다. 이 최적화는 이미 기존 코드에 적용되어 있었습니다.
  2. **중간 변수로 인한 레지스터 압력 증가**: `const uint64_t packed = ...`를 위해 중간 계산 결과를 별도 레지스터에 담아야 했고, 이 추가 레지스터 사용이 핫 루프 내 레지스터 스필(Spill)을 유발했습니다.
  3. **`__builtin_memcpy` 패턴의 부작용**: 컴파일러 입장에서 `__builtin_memcpy(n, &packed, 8)`은 임의의 포인터 복사로 해석될 여지가 있어, 이전 스토어 병합 최적화 기회를 오히려 차단했습니다.
* **가이드라인 (에이전트 수칙)**:
  * **x86_64 에이전트**: TapeNode 같은 `struct {uint32_t meta; uint32_t offset}` 구조에서 두 필드를 개별 대입으로 쓰는 것은 컴파일러가 이미 스토어 병합으로 최적화합니다. `uint64_t packed + __builtin_memcpy` 패턴으로 직접 병합을 강제하지 마세요. 이는 컴파일러 최적화를 방해하고 레지스터 압력을 증가시킵니다.

---

### ❌ [Phase 52] 정수 파싱 AVX2 SIMD 가속 (AVX2 Digit Scanner in kActNumber)
* **목표**: `kActNumber` 처리 경로에서 기존 SWAR-8(8바이트 스칼라 워드) 대신 AVX2 32B 벡터 스캐너를 추가하여 긴 숫자(8자리 초과) 파싱 속도 향상.
  ```cpp
  // 시도: SWAR-8 pre-gate → AVX2 32B bulk
  const __m256i vzero = _mm256_set1_epi8('0');
  const __m256i vnine = _mm256_set1_epi8(9);
  while (p_ + 32 <= end_) {
    __m256i v       = _mm256_loadu_si256(...);
    __m256i shifted = _mm256_sub_epi8(v, vzero);
    __m256i lt0     = _mm256_cmpgt_epi8(_mm256_setzero_si256(), shifted);
    __m256i gt9     = _mm256_cmpgt_epi8(shifted, vnine);
    uint32_t mask   = _mm256_movemask_epi8(_mm256_or_si256(lt0, gt9));
    if (mask) { p_ += __builtin_ctz(mask); goto num_done; }
    p_ += 32;
  }
  ```
* **적용 대상**: `kActNumber` case (line ~6309)
* **실패 결과**:
  * `canada.json`: 1,416μs → 1,374μs (**-2.9%** ← 유일한 개선)
  * `twitter.json`: 365μs → 406μs (**+11.2%** 회귀)
  * `citm_catalog.json`: 955μs → 1,033μs (**+8.1%** 회귀)
  * `gsoc-2018.json`: 751μs → 797μs (**+6.1%** 회귀)
* **근본 원인 분析**:
  1. **YMM 레지스터 충돌**: `kActNumber` 내 `const __m256i vzero/vnine`을 추가하면 `parse()` 함수 전체에서 YMM 레지스터 압력이 급격히 증가합니다. `kActString` 경로의 AVX2 스캐너(`vq`, `vbs` 등 YMM 레지스터)와 충돌하여 컴파일러가 스택 Spill/Reload를 유발했습니다.
  2. **Phase 40과 동일한 메커니즘**: Phase 40 (상수 호이스팅) 실패와 완전히 같은 레지스터 압력 문제입니다. `parse()` 함수 내 두 서로 다른 SIMD 처리 경로가 YMM 레지스터를 공유하면, 컴파일러의 레지스터 할당 예산을 초과합니다.
  3. **숫자 길이 분포 간과**: `twitter.json`의 숫자(18자리 ID)는 드문 케이스입니다. 대부분 짧은 숫자들(<8자리)이 SWAR-8로 빠르게 처리됩니다. AVX2 진입 비용이 실질적 이익보다 큽니다.
* **가이드라인 (에이전트 수칙)**:
  * **x86_64 에이전트**: `parse()` 같은 대형 함수에서 `kActString` 과 `kActNumber` 두 경로 모두 YMM 레지스터 집약 코드를 추가하지 마세요. 두 경로가 동시에 활성화되면 YMM 레지스터 예산(16개)을 초과하여 Spill이 발생합니다. 숫자 파싱 가속이 필요하다면 별도 함수로 분리하여 레지스터 스코프를 격리하는 방식을 검토하세요.

---

## Phase 56-1: LDP (Load Pair) 기반 64B/32B 스칼라-SIMD 융합 공백 스킵 ❌
* **시도 내용**:
  * Apple Silicon 아키텍처 특성에 맞춰 NEON `vld1q_u8` 대신 `load64(p_)` (GPR LDP)를 4회 연속 수행하여 32바이트를 읽는 SWAR 방식을 도입했습니다.
  * `skip_to_action` 내부를 NEON 전주기(16B)에서 SWAR-32B 하이브리드 루프로 전면 교체했습니다.
* **벤치마크 (macOS AArch64 M1 Pro)**:
  * `twitter.json`: 253μs → 275μs (**+8.6%** 회귀)
  * `citm_catalog.json`: 643μs → 836μs (**+30.0%** 회귀)
* **근본 원인 분析**:
  1. **NEON vld1q_u8의 압도적 효율**: Apple CPU는 메모리를 GPR로 LDP하는 것보다, NEON 벡터 레지스터 뱅크로 16B를 직행하는 `vld1q` 파이프라인이 훨씬 더 넓고 빠릅니다.
  2. **SWAR 연산 병목**: 읽어들인 32B를 `swar_action_mask`로 연산하여 비트를 쥐어짜는 GPR ALU 워크로드보다, `vmaxvq_u32`로 SIMD Reduction하는 레이턴시가 훨씬 짧았습니다.
* **가이드라인 (에이전트 수칙)**:
  * **AArch64 에이전트**: 공백 스캔처럼 단순 메모리 밀어주기 작업에 있어서, NEON 벡터 레지스터를 피하고 굳이 범용 레지스터(GPR)로 32B/64B를 읽어 SWAR로 처리하려는 시도는 무조건 성능 하락을 가져옵니다. **단순 필터링은 순수 NEON SIMD 구조를 유지해야 합니다.**

---

## Phase 56-2: NEON 32B Interleaved Branching 문자열 스캐너 ❌
* **시도 내용**:
  * `vgetq_lane` 병목을 회피하면서 32B(16Bx2)를 한 번에 검사하기 위해 2개의 `vld1q_u8`을 인터리빙으로 로드하고 `vceqq_u8` 비교와 `vmaxvq_u32` 축소 검증 사이에 숏서킷 섀도잉 패턴을 적용했습니다.
* **벤치마크 (macOS AArch64 M1 Pro)**:
  * `twitter.json`: 253μs → 250μs (**-1.2%** 미미한 개선)
  * `canada.json`: 1,839μs → 1,855μs (**+0.8%** 회귀)
  * `citm_catalog.json`: 643μs → 655μs (**+1.8%** 회귀)
  * `gsoc-2018.json`: 634μs → 626μs (**-1.2%** 미미한 개선)
* **근본 원인 분析**:
  * Apple Silicon의 분기 예측기와 파이프라인은 단일 16B 순환 루프의 오버헤드를 이미 완벽하게 가리고 있습니다. 32B 수준의 수동 언롤링 및 인터리빙은 추가적인 레지스터 압력만 유발할 뿐, 실질 처리량(Throughput) 개선을 가져오지 못합니다.
* **가이드라인 (에이전트 수칙)**:
  * **AArch64 에이전트**: NEON 스칼라 혼합 루프에 있어서, 32B 이상의 수동 언롤링(Unrolling)은 Apple Silicon에서 효과가 없습니다. 16B 단위 루프가 최적의 분기 예측 효율을 보여줍니다.

---

## Phase 56-5: NEON `scan_key_colon_next` (오브젝트 키 스캐너) ❌ → 🟢 Phase 57에서 "전면 NEON 우세"로 판명됨 (Hypothesis Reversal)
* **초기 시도 내용 (Phase 56)**:
  * AArch64 빌드에서 느린 GPR `SWAR-24`를 대체하기 위해 32B NEON 스캐너를 추가했습니다.
  * 결과: `twitter.json` 253μs → 266μs (**+5.1%** 회귀) 발생. 
  * 초기 결론: "매우 짧은 데이터에 대해서는 Apple Silicon의 분기 예측기와 GPR SWAR 패스가 무거운 NEON 파이프라인보다 압도적으로 빠르다"고 오판하여 코드를 Revert 했습니다.

* **최종 원인 분석 및 발견 (Phase 57 Hypothesis Reversal)**:
  * Phase 57에서 "범용 AArch64(Graviton 등)는 여전히 NEON이 빠를 것이다"라는 가설을 세우고, Apple Silicon용 분기 코드(`__APPLE__`)를 우회하여 강제로 범용 NEON 경로를 태워 벤치마크했습니다.
  * **놀랍게도 `twitter.json` 처리 속도가 266μs나 기존의 253μs가 아닌, 246μs(역대 최고 속도)로 급격히 단축되었습니다.**
  * 추적 결과, Phase 56의 회귀 원인은 NEON 스캐너 탓이 **아니었습니다**. x86_64 AVX-512 최적화에서 차용해왔던 **`skip_to_action` 내부의 "SWAR-8 Pre-gate"** 분기문이 Apple Silicon의 파이프라인을 붕괴시키고 있었던 것입니다.
  * SWAR 스칼라 작업(`EOR` -> `SUB` -> `BIC` -> `AND`) 자체는 레이턴시가 길며 명령어 레벨 병렬성(ILP)이 낮습니다. 반면 NEON의 `vld1q_u8` -> `vcgtq_u8` -> `vmaxvq_u32` 패턴은 AArch64에서 최단 사이클로 완벽하게 병렬 실행됩니다.

* **최종 결론 및 가이드라인 (AArch64 최적화 패러다임)**:
*   **Global NEON Priority**: AArch64 환경(Apple M-Series, AWS Graviton, 단일 Cortex 등 모든 ARM 코어)에서는 SWAR-8/SWAR-24 같은 스칼라 혼합(Pre-gate) 최적화를 **절대 금지**합니다. 
*   "분기 예측을 돕기 위해 스칼라로 8바이트를 먼저 체크한다"는 x86의 논리는 AArch64에서 완전히 실패합니다. 벡터 레지스터 셋업 오버헤드보다, 일관된 벡터명령 파이프라인을 쭉 밀고 나가는 것(`Pure NEON`)이 무조건 가장 빠릅니다.

---

## Phase 60-B: scalar while pre-scan in scan_key_colon_next ❌

* **시도 내용 (2026-03-01)**:
  * `scan_key_colon_next()` 내 `#elif BEAST_HAS_NEON` 블록 앞에 8바이트 스칼라 while 루프를 추가하여 ≤7자 키("id", "text", "user" 등 twitter.json 36%)를 NEON 없이 처리.
  * Phase 56-5와의 차별화 주장: "NEON에 데이터 의존성 없음 — 결과가 goto로만 연결되며 벡터 파이프라인에 GPR 값이 흘러들지 않음."
  ```cpp
  // 시도 코드
  e = s;
  while (e < s + 8 && *e != '"' && *e != '\\') ++e;
  if (e < s + 8) { goto skn_found or skn_slow; }
  // 이후 기존 NEON 블록 실행
  ```
* **벤치마크 결과 (Cortex-X3 pinned, 500 iter)**:
  * baseline (Pure NEON): **243.7 μs**
  * Phase 60-B (8B scalar pre): **257.5 μs** → **+5.6% 회귀** ❌
  * revert 후: 245 μs 복원 확인

* **근본 원인 분析**:
  1. **분기 경쟁 비용**: 스칼라 while 루프(`while (e < s+8 && ...)`)의 8회 반복이 Cortex-X3 정수 파이프라인을 점유. OoO 엔진이 NEON과 병행 실행을 시도하지만, 스칼라 루프의 루프백 분기(8개)가 분기 예측기 슬롯을 소모하여 NEON의 vld1q/vceqq IPC 감소.
  2. **브랜치 미예측 페널티**: 36% 케이스(≤7자 키)에서 스칼라가 일찍 종료("goto skn_found"). CPU는 "not found in 8B → fall through to NEON"을 기본 예측하므로, 36% 케이스에서 ~15사이클 미예측 패널티 발생. Phase 56-5의 실패 원인과 동일.
  3. **Pure NEON 파이프라인 단절**: NEON이 스칼라 while 결과(e 포인터)에 분기 의존성을 가짐. Cortex-X3의 OoO 윈도우가 576 ROB가 아닌 ~200 ROB이므로 분기 해소 전까지 NEON 스페큘레이션이 불완전.

* **결론**: "GPR→SIMD 데이터 의존성 없음"이라는 주장은 이론적으로 맞지만, 분기 의존성(branch dependency) 자체가 NEON 파이프라인을 저해한다. AArch64에서는 NEON과 scalar를 ANY 형태로 혼합하는 hot path 패턴이 Pure NEON보다 느리다. 임계값(8B/16B) 무관하게 동일한 결과 예상.

* **가이드라인 업데이트**:
  * **AArch64 에이전트**: `scan_key_colon_next`의 NEON 블록 앞에 어떤 형태의 스칼라 루프도 추가하지 마세요 — SWAR이든 `while`이든 결과는 동일합니다. "데이터 의존성 없음"이라도 분기 의존성이 NEON 스페큘레이션을 저해합니다.

---

## 🛡️ AArch64 최적화 에이전트 수칙 (Survival Guide)

Termux나 Apple Silicon 환경에서 작업할 차기 에이전트는 아래 수칙을 반드시 준수해야 합니다.

1.  **GPR-SIMD 교차 오염 금지 (Phase 56-5, Phase 60-B 교훈)**:
    - `load64`로 읽어서 비트를 계산한 뒤 그 결과를 바탕으로 SIMD 루프를 결정하는 방식은 파이프라인 버블을 만듭니다.
    - **`while` 루프도 마찬가지**: SWAR-8이 아닌 단순 `while (*e != '"') ++e` 형태의 스칼라 루프도 NEON 앞에 추가하면 동일한 +5~6% 회귀가 발생합니다 (Phase 60-B).
    - "GPR→SIMD 데이터 의존성 없음"이라도 분기 의존성이 NEON 스페큘레이션을 저해합니다.
    - 특히 `skip_to_action`이나 `scan_key_colon_next` 같은 초고속 핫 패스에서는 **처음부터 100% 벡터 명령**만 사용하세요.
2.  **분기 예측기(Branch Predictor) 신뢰**:
    - AArch64의 분기 예측기는 매우 강력합니다. 16바이트 벡터 검사 후 일치하는 바이트를 찾기 위해 수행하는 `while` 루프(스칼라)는 페널티가 거의 없습니다. 
    - 이를 굳이 비트 연산으로 해결하려고 `vgetq_lane` 같은 명령을 사용하여 GPR로 옮기는 순간, 수 사이클의 지연이 발생합니다.
3.  **언롤링(Unrolling) 주의**:
    - 16바이트 단일 루프만으로도 Apple M1/Snapdragon 8급의 OoO 엔진은 이미 충분한 처리량을 뽑아냅니다. 
    - 32B/64B 수동 언롤링은 코드 크기만 키우고(I-cache 압박) 실질 이득은 미미하거나 오히려 마이너스가 될 수 있습니다 (Phase 56-2 참조).
4.  **Snapdragon 8 Gen 2 (Fold 5) = Standard ARM Proxy**:
    - Snapdragon 8 Gen 2는 표준 Cortex-X3 코어를 사용합니다. 이는 AWS Graviton 3(Neoverse V2)와 사실상 형제 모델입니다.
    - 애플 전용 최적화(`__APPLE__`)에 매몰되지 마세요. Phase 57에서 증명했듯, 우리의 **Pure NEON** 전략은 애플과 표준 ARM 모두에서 승리하는 유일한 "범용 아키텍처 중립" 코드입니다.
    - Snapdragon 환경에서 속도가 떨어진다면, 그것은 알고리즘의 문제이지 아키텍처의 문제가 아닐 가능성이 큽니다.

---

## Phase 63: AArch64 32B 듀얼 체크 skip_to_action ❌

* **시도 내용**: `skip_to_action()` NEON 루프를 16B → 32B로 확장. v1=[p_, p_+16), v2=[p_+16, p_+32)를 동시 로드. m1 먼저 체크(단거리 WS 즉시 종료), 그다음 m2 체크(p_+=16 후 종료). 두 블록 모두 공백이면 p_+=32.

* **예상**: m2 검사가 병렬 실행 → 반복 횟수 반감 → 긴 WS 구간(canada.json) -5% 이상.

* **실제 결과** (5회 A/B, Phase 62 대비):
  * twitter: **+3.2% 회귀** (245.6μs → 253.5μs)
  * citm: +0.7% (노이즈 수준)
  * gsoc: +1.1% (노이즈 수준)

* **원인 분석**:
  1. twitter.json은 WS 런이 1-3바이트 → 16B 루프에서 이미 첫 블록 m1에서 즉시 종료.
  2. 32B 버전은 m1 판정 후에도 m2(VLD1Q + VCGTQ + VMAXVQ)를 추가로 실행 → issue slot 소비.
  3. 이 추가 비용이 반복 횟수 절감 효과를 초과.
  4. Pure NEON 패러다임 원칙: 단거리 WS가 지배하는 파일에서 추가 벡터 연산 비용이 오히려 증가.

* **결론**: skip_to_action의 32B 확장은 "긴 연속 WS"가 지배하는 파일에서만 유리하며, 실제 JSON 파일의 WS 패턴(대부분 1-3바이트)에서는 순수 회귀. 16B NEON 루프가 최적.

---

---

## M1 Phase 72-81: Apple Silicon serialize LTO/I-cache 황금 규칙 확립 과정

> **배경**: Phase 72-M1부터 serialize 최적화를 본격 시작. Phase 80-M1로 완성. 이 과정에서 M1 PGO+LTO 환경 특유의 극단적 민감성을 발견하고 황금 규칙을 확립.

### ❌ Phase 73-M1: vshrn+ctz SWAR 대체 시도

**목표**: `scan_string_end()` 내 scalar `while` 루프를 `vshrn_n_u16` + `__builtin_ctzll`로 교체하여 NEON 16B 단위로 첫 특수문자 위치를 O(1) 계산.

**실패 원인**: M1의 scalar while 루프는 분기 예측기+OoO 실행으로 이미 최적. `vshrn` 기반 movemask 에뮬레이션(6+ instructions) + GPR-SIMD 전송 레이턴시가 scalar loop보다 더 느림.

**교훈**: M1에서 "NEON movemask 없음" 문제는 x86의 bsf/tzcnt와 달리 에뮬레이션 비용이 scalar보다 높다. scalar `while`을 NEON CTZ로 교체하려는 시도 금지.

---

### ❌ Phase 74-M1 / 74B-M1: serialize 코드 추가 (uint64 변환 NEON화)

**Phase 74-M1**: value string 3×16B 스캐너 + serialize의 uint64/int64 변환 NEON화
**Phase 74B-M1**: serialize uint64/uint32만 단독으로 NEON화 (parse 변경 제외)

**실패 결과**:
- canada serialize: +11% 회귀
- citm serialize: +11% 회귀

**근본 원인**: serialize 함수에 **코드 추가** → 전체 함수 크기 증가 → I-cache 압력 증가 → PGO+LTO가 현재 레이아웃을 깨고 재배치 → 회귀.

**핵심 발견**: serialize 코드 추가는 parse 코드와 무관해도 serialize 자체에 회귀를 유발한다. I-cache 압력이 LTO 재배치와 독립적으로 직접 영향.

**교훈**: M1 serialize 함수에 코드 추가 = 절대 금지. Phase 74B로 parse 변경을 제거해도 동일 회귀 → serialize 코드 크기 자체가 문제.

---

### ❌ Phase 76(1차)-M1: parse 코드 제거 (key scanner fallback 제거)

**시도**: `scan_key_colon_next()` 내 2×16B NEON fallback 경로 제거 (dead code 정리 목적)

**실패 결과**:
- citm serialize: +6μs 회귀

**근본 원인**: parse 함수의 코드 **제거** → 기본 블록 구조 변경 → PGO+LTO 코드 레이아웃 재배치 → serialize 함수 배치 영향.

**핵심 발견**: parse와 serialize는 동일 LTO 단위에 있어, parse의 어떤 변경도 serialize 레이아웃에 영향. 코드 제거도 추가만큼 위험.

**교훈**: parse 코드 제거 = LTO 교란 → serialize 회귀. Phase 77B, 78에서 동일 패턴 재확인.

---

### ❌ Phase 77-M1: serialize 코드 추가 (32-48B NEON inline)

**시도**: StringRaw case에서 slen 32-48B 범위에 3×16B NEON overlapping triple 추가

**실패 결과**:
- citm serialize: 174μs → 213μs (+22% 회귀)
- canada serialize: 716μs → 839μs (+17% 회귀)

**근본 원인**: serialize 함수에 코드 추가 → I-cache footprint 증가 → citm의 26,604 string 노드 처리 시 L1 I-cache miss 증가.

**수치 분석**: citm 95.1%가 slen 1-16B (fast path). 32-48B 최적화는 4.9% 케이스에만 적용되나, 추가된 코드가 fast path의 I-cache 효율을 저하. 코드 추가의 비용이 이득을 초과.

**교훈**: serialize 코드 추가는 실제로 사용되는 코드라도 I-cache 압력으로 회귀. "사용하지 않는 코드만 위험하다"는 가정 틀림.

---

### ❌ Phase 77B-M1: parse 코드 제거 (parse_number single-digit 최적화 제거)

**시도**: `parse_number()` 내 single-digit 빠른 경로 조건 분기 제거 (코드 정리)

**실패 결과**:
- twitter serialize: 67μs → 101μs (+50% 파국적 회귀)

**근본 원인**: parse 코드 제거 → LTO 재배치 → serialize 레이아웃 교란. Phase 76(1차)와 동일 패턴.

**교훈**: parse 코드 제거가 serialize에 +50%를 유발. LTO 단위 내에서 함수 배치 의존성이 얼마나 취약한지 확인. parse와 serialize 분리 없이는 어떤 parse 변경도 serialize를 위협한다.

---

### ❌ Phase 78-M1: parse 코드 추가 (scan_string_end 32B outer loop)

**시도**: `scan_string_end()` 내 16B inner loop 바깥에 32B/iteration outer loop 추가

**실패 결과**:
- twitter serialize: 99.87μs (Phase 80 이전 상태, 67μs 대비 +50% 회귀)

**근본 원인**: parse 코드 추가 → LTO 교란 → serialize 레이아웃 변경. Phase 74, 77 serialize 추가와 동일 패턴이지만 parse 쪽 추가임.

**교훈**: parse 코드 추가 = LTO 교란 → serialize 회귀. 양방향 모두 위험 확인.

---

### ❌ Phase 81-M1: StringRaw early-continue before switch

**시도**: serialize 루프 내 `for` 문에서 switch 이전에 StringRaw 조건 분기 + `continue` 추가:
```cpp
// Phase 81 시도 (원복됨)
if (BEAST_LIKELY(type == TapeNodeType::StringRaw)) {
    // ... StringRaw fast path ...
    continue;  // ← 이 continue가 문제
}
switch (type) {
    // StringRaw case는 #if !BEAST_ARCH_APPLE_SILICON로 제거
    ...
}
```

**실패 결과** (fresh PGO pgo_67777.profraw 기준):
- citm serialize: 166μs → 196μs (+19% 회귀)
- canada serialize: 701μs → 916μs (+31% 회귀)

**근본 원인**: `continue` 문이 `for` 루프에 두 번째 back-edge를 생성:
- 변경 전: 루프 back-edge 1개 (switch 끝 → 루프 선두)
- 변경 후: 루프 back-edge 2개 (continue 분기 + switch 끝 → 루프 선두)
- LTO 컴파일러가 multiple back-edge를 다르게 해석 → 루프 최적화 전략 변경 → 코드 레이아웃 교란 → 회귀

**이론 vs 현실**: StringRaw가 95%+ citm 노드이므로 indirect jump(switch) → direct branch(if) 교체는 이론적으로 유리. 그러나 loop 구조 변경이 LTO의 inlining/layout 결정을 바꾼다.

**교훈**: 루프 내 `continue`/`break` 추가로 새 back-edge를 만드는 모든 변경 = LTO 교란. 새 황금 규칙으로 등록.

**복구**: `git checkout include/beast_json/beast_json.hpp` + `xcrun llvm-profdata merge pgo_65861.profraw -o pgo.profdata`

---

## Phase 73: `dump(string&)` buffer-reuse ✅ **성공** (Phase 66-B 실패의 후속)

> 이 섹션은 실패 기록이 아닌 **성공 사례**이지만, Phase 66-B 실패 기록과 직접 연결되어 있어 여기에 함께 기술합니다.

### 배경 (Phase 66-B의 실패 원인 재검토)

Phase 66-B는 동일한 `dump(string&)` 오버로드를 x86_64 + PGO + LTO 환경에서 시도했다가 파서 회귀를 유발했습니다. 실패 원인은 크게 두 가지였습니다:

1. **NOINLINE 속성 사용**: `BEAST_NOINLINE void dump(std::string& out) const` 형태로 구현 → I-cache 레이아웃 변경 → GCC LTO 단위 내에서 파서 코드 재배치 → citm parse +21% 회귀.
2. **x86_64 PGO/LTO 환경**: serialize 루프와 parse 코드가 동일 LTO 단위에서 코드 레이아웃 공유. 어떤 serialize 변경도 간접적으로 파서에 영향.

### Phase 73의 해결 방법

* **NOINLINE 없음**: 두 오버로드(`dump()`, `dump(string&)`)를 완전히 독립적으로 구현 (코드 복제). 컴파일러가 두 call site 모두 인라인 → I-cache 레이아웃 안정.
* **AArch64 + `-fno-lto` 환경**: Snapdragon/Termux 빌드는 이미 LTO 없이 빌드 (`-fno-lto -fno-vectorize -fno-slp-vectorize`). PGO/LTO 크로스 컨테미네이션 없음.
* **`__resize_default_init` (libc++ 전용)**: `resize(n)`는 새 바이트를 0으로 초기화하지만, `__resize_default_init(n)`은 크기 필드만 업데이트(O(1)). libstdc++ 환경은 `resize(n)` 폴백(첫 호출에만 memset 발생, 이후 shrink+expand 시에도 덮어쓰기).

### 결과 (2026-03-03, Snapdragon Cortex-X3, bench_ser_profile 1000 iter)

| 파일 | 구 `dump()` | 신 `dump(string&)` | 개선 |
|:---|---:|---:|:---:|
| twitter.json | 170 μs | **89 μs** | **-47.7%** |
| canada.json | 778 μs | **471 μs** | **-39.5%** |
| citm_catalog.json | 466 μs | **208 μs** | **-55.4%** |
| gsoc-2018.json | 694 μs | **197 μs** | **-71.6%** |

**교훈**: Phase 66-B는 구현 방식(NOINLINE + LTO 환경) 문제였고, 아이디어 자체(buffer-reuse)는 정확했다. Snapdragon AArch64 + `-fno-lto` 환경에서는 코드 복제 + 인라인 전략이 완벽하게 작동했다.
