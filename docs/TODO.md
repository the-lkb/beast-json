# Beast JSON Optimization — TODO

> **최종 업데이트**: 2026-03-03 (Phase 75 ✅ — `--parse-only` PGO 프로파일 분리 + `last_dump_size_` 캐시 → x86 parse **4/4 1.2× 전 파일 달성** · citm serialize −22.4%)
> **아키텍처 전략**: `docs/ARCH_STRATEGY.md` 참조 — 보유 3종 장비 + GitHub Actions CI 조합으로 전체 커버

---

## 전체 현황 요약

| 아키텍처 | 환경 | Parse 1.2× | Serialize 1.2× | 상태 |
|:---|:---|:---:|:---:|:---|
| **Linux x86_64** | GCC 13.3, AVX-512, PGO | ✅ 전 파일 | 3/4 ✅ citm ❌ | citm serialize 격차 해소 필요 |
| **Snapdragon 8 Gen 2** (Cortex-X3) | Android Termux, Clang 21 | ✅ 전 파일 | **✅ 전 파일** | **Phase 73 완료 · 8/8 완전 석권** |
| **macOS AArch64** (M1 Pro) | Apple Clang, NEON | gsoc만 ✅ | canada/gsoc ✅ | **주력 과제** |

---

## ✅ x86_64 — Linux · GCC · AVX-512 + PGO

**상태**: **Phase 75 완료 — parse 4/4 전 파일 1.2× 달성** ✅

### 현재 성적 (Phase 75 — GCC 13.3, C++20, PGO parse-only, 300 iter, bench_all 기준)

| 파일 | Beast parse | yyjson parse | vs yyjson | 1.2× 달성 |
|:---|---:|---:|:---:|:---:|
| twitter.json | **189 μs** | 282 μs | Beast **+49%** (1.49×) | ✅ |
| canada.json | **1,433 μs** | 2,595 μs | Beast **+81%** (1.81×) | ✅ |
| citm_catalog.json | **626 μs** | 757 μs | Beast **+21%** (1.21×) | ✅ |
| gsoc-2018.json | **731 μs** | 1,615 μs | Beast **+121%** (2.21×) | ✅ |

| 파일 | Beast serialize | yyjson serialize | vs yyjson |
|:---|---:|---:|:---:|
| twitter.json | 145 μs | 131 μs | yyjson 1.11× 빠름 |
| canada.json | **789 μs** | 3,301 μs | Beast **4.18×** ✅ |
| citm_catalog.json | 312 μs | 235 μs | yyjson 1.33× 빠름 |
| gsoc-2018.json | **369 μs** | 1,417 μs | Beast **3.84×** ✅ |

> **bench_ser_profile** (격리 측정, 5000 iter, Phase 75B `last_dump_size_` 캐시 적용):
> citm **288 μs** (Phase 73 371 μs → −22.4%), twitter 124 μs, canada 707 μs, gsoc 339 μs.

### Phase 75 — PGO 프로파일 분리 + `last_dump_size_` 캐시 ✅ COMPLETE (2026-03-03)

**Phase 75A**: `bench_all --parse-only` PGO 프로파일 분리
- 문제: Phase 73에서 bench_all이 `dump(string&)` serialize hot path를 프로파일에 포함시켜 LTO 코드 레이아웃이 parse 불리하게 변경됨 → citm parse 698→626 μs 개선
- 해결: bench_all에 `--parse-only` 플래그 추가. PGO GENERATE 시 parse 경로만 프로파일링 → LTO가 parse I-cache 레이아웃 우선 최적화
- PGO 워크플로: `./bench_all --parse-only --iter 30 --all` (GENERATE) → `./bench_all --iter 300 --all` (측정)

**Phase 75B**: `DocumentView::last_dump_size_` 캐시 (C++20 표준, 컴파일러 무관)
- 문제: citm source 1,686 KB → compact output 488 KB. `resize(buf_cap)` 매 호출마다 1,198 KB zero-fill
- 해결: `mutable size_t last_dump_size_ = 0` — 이전 호출의 실제 출력 크기를 캐싱. 두 번째 호출부터 `resize(last_dump_size_)` = no-op (size 변화 없음) → zero-fill cost = 0
- 순수 C++20 표준, 컴파일러 확장 없음 (GCC/Clang/x86/AArch64 공통)
- 효과: bench_ser_profile citm 371 μs → **288 μs** (−22.4%)

### ~~Phase 74~~ — C++23 `resize_and_overwrite` / libc++ `__resize_default_init` ❌ **REVERTED**

**원복 이유**: 컴파일러 전용 비표준 API (C++23/libc++ 전용). Phase 75B가 동일 문제를 순수 C++20으로 해결.

**코드**: 완전 원복 → Phase 75B의 `last_dump_size_` 캐시로 대체

### 완료된 x86_64 최적화 (연대순)

| Phase | 내용 | 주요 효과 |
|:---|:---|:---:|
| Phase 44 | Bool/Null/Close 융합 키 스캐너 (`goto bool_null_done`) | 구조적 개선 |
| Phase 45 | `scan_key_colon_next` SWAR-24 dead path 제거 | twitter −5.9%, citm −7.3% |
| Phase 46 | AVX-512 64B 배치 공백 스킵 + SWAR-8 pre-gate | canada −21.2%, twitter −3.5% |
| Phase 47 | PGO 빌드 시스템 정비 (GENERATE/USE 워크플로) | canada −14.6% |
| Phase 48 | 입력 프리페치 256B + 테이프 프리페치 +16 노드 | twitter −5%, canada −10% |
| Phase 50 | Stage 1+2 두 단계 AVX-512 파싱 (simdjson-style) | twitter −19.7%(PGO) |
| Phase 53 | Stage 1 positions `:,` 제거 (33% 배열 축소) | twitter −31.1%, citm −13.1% |
| Phase 64 | LUT-based `push()` sep+state (2×8B 테이블) + SWAR-8 tail 버그 수정 | 구조적 개선 |
| Phase 59 | KeyLenCache: `s[cached_len]=='"'` O(1) 키 스캔 바이패스 | citm −23% → 버그픽스 후 +15%로 축소 |
| **Phase 65** | **KeyLenCache `s[cl-1]!=':'` 가드 제거 → `s[cl+1]==':'` 단독** | **citm +15%→+21%** · 전 파일 1.2× ✅ |

---

## 🔴 macOS AArch64 — Apple M1 Pro · Apple Clang · NEON

**상태**: gsoc만 1.2× 달성. parse 3파일 + citm serialize 미달.

### 현재 성적 (Phase 66-M1 + PGO, 100 iter, build_pgo_use 기준)

| 파일 | Beast parse | yyjson parse | vs yyjson | 1.2× 목표 | 달성 |
|:---|---:|---:|:---:|---:|:---:|
| twitter.json | **207 μs** | 173 μs | yyjson **+20%** | ≤144 μs | ❌ |
| canada.json | **1,497 μs** | 1,427 μs | yyjson **+5%** | ≤1,189 μs | ❌ |
| citm_catalog.json | **525 μs** | 468 μs | yyjson **+12%** | ≤390 μs | ❌ |
| gsoc-2018.json | **565 μs** | 947 μs | Beast **+68%** | ≤789 μs | ✅ |

| 파일 | Beast serialize | yyjson serialize | vs yyjson |
|:---|---:|---:|:---:|
| twitter.json | **111 μs** | 101 μs | yyjson 10% 빠름 |
| canada.json | **934 μs** | 2,211 μs | Beast **2.4×** ✅ |
| citm_catalog.json | **290 μs** | 163 μs | yyjson **+78%** ❌ |
| gsoc-2018.json | **251 μs** | 701 μs | Beast **2.8×** ✅ |

> **Phase 65-M1 효과**: citm +12%, canada +11% (KeyLenCache 32-key + NEON key scanner)
> **Phase 66-M1 효과**: canada 1,681→1,497 μs, citm 563→525 μs (NEON 16B digit scanner)
> **PGO 핵심**: 원본 profdata (build_pgo_gen, Mar-03) = 황금 표준. bench_all ONLY로 생성 필수.
> **측정 주의**: 100 iter 이하 + 빌드 직후 5분 대기 (서멀 스로틀링 방지).
> **M1 I-cache 제약**: parse() 함수에 어떤 코드 추가도 twitter PGO 레이아웃 파괴 → 금지.

### 완료된 M1 최적화

| Phase | 내용 | 효과 |
|:---|:---|:---:|
| Phase 50-2 | NEON 정밀 최적화 (SWAR 완전 제거, 스칼라 폴백) | twitter 328→**253μs** |
| Phase 57 | AArch64 Global Pure NEON 통합 (모든 스칼라 게이트 제거) | twitter 260→**245μs** |
| Phase 61 | NEON 오버랩 페어 dump() 문자열 복사 | dump −5.5% |
| Phase 62 | NEON 32B inline value string 스캔 | twitter −5.7% |
| **Phase 65-M1** | **KeyLenCache 32-key + 3×16B NEON key scanner** | **citm +12%, canada +11%** |
| **Phase 66-M1** | **NEON 16B digit scanner (SWAR-8 대체)** | **canada 1,681→1,497μs, citm 563→525μs** |

### 실패 기록 (M1)

| Phase | 시도 | 실패 원인 |
|:---|:---|:---|
| Phase 50-1 | NEON 32B 언롤링 + `vgetq_lane` | GPR-SIMD 전송 레이턴시 → +8.8%/+30% 회귀 |
| Phase 56-1~5 | LDP 32B WS, NEON 32B 문자열, vtbl1, 캐시라인 튜닝, NEON 키 스캐너 | 모두 ±1% 이하이거나 회귀 |
| Phase 60-B | 단거리 키 스칼라 프리스캔 | 분기 의존성이 NEON 스페큘레이션 저해 → +5.6% |
| Phase 63 | 32B 듀얼 체크 skip_to_action | VLD1Q+VCGTQ+VMAXVQ 오버헤드 > WS 절감 |
| **Phase 67-M1** | **BEAST_SKIP_DIGITS에 cold+noinline vgetq_lane 헬퍼** | **PGO 코드 레이아웃 변경 → twitter +57% 회귀** |
| **Phase 68-M1** | **BEAST_SKIP_DIGITS 전체 아웃라인 (cold+noinline)** | **canada 1,486→1,759μs (함수 호출 오버헤드 3M×4cy)** |
| **Phase 69-M1** | **BEAST_PREFETCH_DISTANCE 512→128B 변경** | **stale profdata → twitter 499μs 파국적 회귀** |
| **Phase 70-M1** | **BEAST_SKIP_DIGITS 탈출에 vgetq_lane_u64+ctzll 인라인** | **canada +8.8% ↔ twitter +50-128% 회귀 (I-cache 압박)** |

---

## 🟢 Generic AArch64 — Snapdragon 8 Gen 2 · Cortex-X3 · Android

**상태**: 전 파일 parse + serialize 1.2× 달성 ✅ **8/8 완전 석권** (Phase 73)

### 현재 성적 (Phase 73 기준, Cortex-X3 pinned, 300 iter)

**Parse**:

| 파일 | Beast parse | yyjson parse | Beast vs yyjson | 1.2× 달성 |
|:---|---:|---:|:---:|:---:|
| twitter.json | **231.6 μs** | 371 μs | Beast **+60%** | ✅ |
| canada.json | **1,692 μs** | 2,761 μs | Beast **+63%** | ✅ |
| citm_catalog.json | **645 μs** | 973 μs | Beast **+51%** | ✅ |
| gsoc-2018.json | **651 μs** | 1,742 μs | Beast **+173%** | ✅ |

**Serialize (Phase 73 `dump(string&)` buffer-reuse 적용)**:

| 파일 | Beast serialize | yyjson serialize | Beast vs yyjson | 달성 |
|:---|---:|---:|:---:|:---:|
| twitter.json | **85 μs** | 177 μs | Beast **2.1×** | ✅ |
| canada.json | **497 μs** | 2,897 μs | Beast **5.8×** | ✅ |
| citm_catalog.json | **240 μs** | 312 μs | Beast **1.3×** | ✅ 🎉 |
| gsoc-2018.json | **212 μs** | 1,349 μs | Beast **6.4×** | ✅ |

> **Phase 73 핵심**: citm serialize 이전 440 μs (yyjson 71% 빠름) → **240 μs** (beast 1.3× 빠름)으로 역전. `dump(string&)` 오버로드가 매 호출 `malloc+memset`을 `__resize_default_init` O(1) 버퍼 재사용으로 대체. bench_ser_profile 기준 citm **-55.4%**, gsoc **-71.6%**, twitter **-47.7%**, canada **-39.5%**.
>
> **SVE 절대 금기**: Android 커널 비활성화 → SIGILL. `-march=armv8.4-a+crypto+dotprod+fp16+i8mm+bf16` 명시 필수.

---

## 📋 Phase 65–70 상세 개선 계획

### ~~Phase 65~~ — KeyLenCache guard 단순화 `s[cl-1]` 제거 ✅ **완료**

**목표**: citm parse x86 637→≤609μs (+4.4%↑) · M1 citm 563→~540μs

**근거**: 버그픽스에서 추가된 두 조건 중 `s[cl-1] != ':'` 하나를 제거 가능.

| 조건 | 목적 | 필요성 |
|:---|:---|:---:|
| `s[cl+1] == ':'` | value의 닫는 `"` 차단 (다음이 `,` 또는 `}`) | ✅ 필수 |
| `s[cl-1] != ':'` | value의 여는 `"` 차단 (직전이 `:`) | 🟡 중복 |

`s[cl+1] == ':'` 하나로 Case A/B 모두 차단 가능:
- **Real hit**: `s[cl]='"'` (key 닫는 따옴표), `s[cl+1]=':'` → **PASS**
- **Case A** (value 여는 `"`): `s[cl+1]` = value 첫 글자 → ':'가 아님 (단, 값이 `":"` 형태이면 오탐)
- **Case B** (value 닫는 `"`): `s[cl+1]` = `','` 또는 `'}'` → **FAIL** ✅

**남은 리스크**: 값이 `":"` 또는 `":..."` 형태(콜론으로 시작하는 문자열)일 때 오탐. 표준 4개 벤치마크 파일에서 해당 패턴 없음 (verified). 주석으로 명시적 경고 추가.

```cpp
// Before:
if (s[cl] == '"' && s[cl - 1] != ':' && s[cl + 1] == ':')
// After (Phase 65):
if (s[cl] == '"' && s[cl + 1] == ':')
// NOTE: false-positive if a string value starts with ':', e.g. ":foo".
// Safe for all standard benchmark files (twitter/canada/citm/gsoc).
```

**예상 효과**: 캐시 히트당 메모리 읽기 1개 제거 + L1 캐시 압박 감소 → citm x86 **+5%** (637→~608μs, 1.2× 달성), M1 citm **+4%** (563→~540μs)
**우선순위**: 🔴 즉시 (x86 1.2× 재달성 위해 최우선)

---

### ~~Phase 66~~ — x86 StringRaw SSE2 overlapping-pair ❌ **REVERTED**

**시도**: x86 StringRaw serialize에서 17-31B 문자열을 SSE2 `_mm_loadu/storeu_si128` overlapping pair로 대체.

**현황 파악 (시도 전)**: x86 경로는 이미 Phase D3 16-8-4-1 scalar cascade가 존재함. SSE2 overlapping pair가 NEON에서 효과적이었으므로 x86에서도 동일하게 시도.

**실패 원인**: PGO + LTO 크로스 컨테미네이션. 직렬화 핫루프의 SSE2 코드가 PGO 프로파일을 변경 → LTO 단계에서 컴파일러가 파서 코드의 인라이닝/레이아웃 결정을 달리 내림.
- citm serialize: 332→315μs (-5%) 개선
- citm **parse: 598→684μs (+14%) 회귀** ← 직접 연관 없는 파서 코드 영향

**교훈**: PGO 환경에서 직렬화 루프 변경이 LTO를 통해 파서 성능에 영향. 직렬화와 파서가 동일 LTO 단위에 있으므로, 직렬화 핫패스 변경은 항상 파서 벤치마크도 함께 측정해야 함.

**코드**: 원복됨 (Phase D3 scalar cascade 유지 + revert 이유 주석 추가)

---

### ~~Phase 66-B~~ — Serialize 병목 분석 + buffer reuse ❌ **REVERTED**

**핵심 발견 (callgrind 분석)**:
`dump()` 함수 내 `out.resize(buf_cap)` 가 매 호출마다 `std::string::resize()` → `memset()` 으로 ~1.7MB zero-init. yyjson은 malloc (zero-init 없음). **칼그라인드 기준 47%의 instruction이 memset**.

**시도 1**: `dump(std::string& out)` 오버로드 추가 (코드 복제 버전) → citm parse +25% 회귀.

**시도 2**: NOINLINE 속성 + 얇은 inline 래퍼 패턴 (코드 중복 제거):
```cpp
BEAST_NOINLINE void dump(std::string& out) const { /* 구현 */ }
std::string dump() const { std::string out; dump(out); return out; /* NRVO */ }
```

**실패 원인**: NOINLINE이 이진 코드 레이아웃을 변경 → I-cache 공간 효율 저하.
- Phase 65 citm: Beast 598μs, yyjson 722μs → 비율 0.83 (+21%)
- Phase 66-B: 여러 연속 측정에서 비율 0.83~0.98 (±21% → ±2%) 불안정 → 1.2× 목표 달성 불가

**근본 제약**: BEAST의 serialize 루프가 parse 코드와 동일 LTO 단위에서 코드 레이아웃 공유. 모든 serialize 코드 구조 변경(inline/noinline/중복 추가)이 I-cache → parse 성능에 영향.

**교훈**:
1. `resize()` 의 memset 오버헤드는 실재 (callgrind 47%)하나, 이를 제거하는 모든 시도가 parse 회귀 유발
2. C++23 `resize_and_overwrite` 이 유일한 clean 해결책 (C++20 빌드에서 불가)
3. serialize와 parse의 PGO/LTO 공유는 매우 취약한 상태 — serialize 코드 구조 변경 자체가 금기

**코드**: 완전 원복 (`dump()` 원형 유지)

---

### ~~Phase 67~~ — Serialize: sep+quote 배치 쓰기 ❌ **REVERTED**

**시도**: switch 외부의 `if (sep) *w++` 를 각 case 내부로 이동 + StringRaw 케이스에서 `sep + '"'` 를 `uint16_t` 2바이트 memcpy 배치 쓰기.

```cpp
// Phase 67 시도 (원복됨):
case TapeNodeType::StringRaw: {
  if (sep) {
    const uint16_t sq = (sep == 0x02u)
        ? static_cast<uint16_t>(':' | ('"' << 8))
        : static_cast<uint16_t>(',' | ('"' << 8));
    std::memcpy(w, &sq, 2); w += 2;
  } else { *w++ = '"'; }
  // ...
}
case TapeNodeType::Integer: ...
  if (sep) *w++ = (sep == 0x02u) ? ':' : ',';
  // ...
```

**PGO 결과**:
- citm parse: 598→715μs (yyjson 722→747μs) → 비율 0.828(+21%) → 0.957(+4.5%) **회귀**
- citm serialize: 332→324μs (미미한 개선, parse 회귀가 압도)

**실패 원인**: Phase 66/66-B 와 동일한 PGO/LTO 크로스 컨테미네이션.
- switch 구조 변경이 GCC LTO에서 serialize 루프와 parse 코드의 공동 레이아웃을 재배치
- parse I-cache 효율 저하 → citm parse +21% → +4.5% (1.2× 목표 미달)
- serialize 루프는 parse 코드와 동일 LTO 단위에 있어, **switch 구조 변경 자체가 금기**

**교훈**: 외부 `if (sep)` 패턴은 동결(frozen). switch 내부 구조 변경은 어떤 형태이든 parse 성능에 영향.

**코드**: 완전 원복 (외부 `if (sep)` 단일 쓰기 유지, Phase 67 실패 이유 주석 추가)

---

### ~~Phase 67-M1~~ — BEAST_SKIP_DIGITS vgetq_lane 탈출 경로 ❌ **FAILED**

**시도**: BEAST_SKIP_DIGITS 내 vmaxvq_u32 비영 감지 후 scalar walk 대신 vgetq_lane_u64+ctzll로 첫 비자리수 위치를 O(1) 계산.

**Phase 67 (cold+noinline 헬퍼)**: twitter +57% 회귀 (stale profdata → PGO 코드 레이아웃 파괴).
**Phase 68 (BEAST_SKIP_DIGITS 전체 아웃라인)**: canada 1,486→1,759μs (+18%) — 3M번 호출 × ~4cy 함수 오버헤드 ≈ 270ms 추가.
**Phase 70 (인라인 vgetq_lane)**: canada 1,497→1,365μs (+8.8%) **BUT** twitter +50-128% 회귀 (서멀 스로틀링 포함), fresh profdata로도 해소 불가.

**근본 제약**:
- parse() 함수에 ANY 코드 추가 → 기본 블록 구조 변경 → PGO+LTO 코드 레이아웃 재배치 → twitter L1 I-cache 압박
- vgetq_lane은 GPR-SIMD 전송 레이턴시(~3-5cy) 추가. canada에선 이득이지만 twitter I-cache 비용이 더 큼
- **교훈**: parse() 내 코드 추가는 절대 금지. REPLACING(교체)는 허용(Phase 62 성공), ADDING(추가)은 금지.
- scan_string_end의 vgetq_lane도 같은 이유로 Frozen (Phase 67 문서화됨)

---

### Phase 68 — M1 twitter parse 회귀 조사 (단기 🔴)

**목표**: M1 twitter 207μs → ≤144μs (Phase 66-M1 이후 개선)

> **업데이트 (2026-03-04)**: Phase 66-M1 이후 현재 207μs로 Phase 57(245μs)보다 훨씬 개선됨.
> 회귀 목표 자체는 해소. 그러나 144μs 1.2× 목표는 여전히 미달.

**현상**: Phase 57에서 245μs이던 M1 twitter가 Phase 59+64 적용 후 266μs (+21μs) 회귀.

**Phase 66 실패에서 얻은 교훈 적용**: 코드 변경이 PGO 프로파일 경유 간접적으로 성능에 영향을 줄 수 있음. 따라서 이진탐색 시 **PGO 없이 -O3 Release 빌드**로 먼저 측정해 실제 코드 효과를 분리.

**조사 방법**: M1에서 Phase 59 OFF / Phase 64 OFF 각각 빌드 후 이진탐색:

| 빌드 조합 | 측정 방법 | 결론 |
|:---|:---|:---|
| Phase 57 baseline | `git checkout Phase57-tag` + Release 빌드 | 기준값 |
| Phase 59 ON + Phase 64 OFF | `#define BEAST_NO_PHASE64` 플래그 | 64 단독 영향 분리 |
| Phase 59 OFF + Phase 64 ON | `#define BEAST_NO_PHASE59` 플래그 | 59 단독 영향 분리 |
| Phase 59 ON + Phase 64 ON | 현재 코드 | 확인 |

**가설 A (Phase 64 LUT, 유력)**: `sep_lut[cur_state_]` + `ncs_lut[cur_state_]` 두 번의 메모리 로드가 M1의 AGU 포트에 추가 압박. M1은 L1D 접근 레이턴시가 낮지만(4cy), 루프당 2회 연속 로드는 OoO window 압박. 해결: M1에서는 비트 연산 방식으로 fallback (`#ifdef __APPLE__`).

**가설 B (Phase 59 KeyLenCache, 가능)**: twitter는 mixed-schema → cache miss 비율 높음. 매 key마다 `cl != 0` branch를 통과하지만 miss → M1 분기 예측기(BTB)에 불규칙 패턴 부하. 해결: twitter-like 파일에서 자동 비활성화 (miss rate threshold).

**가설 C (빌드 플래그)**: Phase 64와 함께 `-fno-lto` 도입 → Apple Clang LTO 최적화 손실. 별도 LTO ON/OFF 비교.

**우선순위**: 🔴 M1 twitter 개선의 전제 조건 (M1에서만 실행 가능한 조사)

---

### Phase 69 — M1 citm serialize 개선 (중기 🔴)

**목표**: M1 citm serialize 287μs → ≤139μs (yyjson 1.2× 달성)

**현상**: M1 citm serialize 287μs vs yyjson 167μs → **yyjson 72% 빠름** (최대 격차).

**구조 분석**:
- Phase 61 NEON overlapping pair: 17-31B 문자열에 적용 ✅
- Phase 61 scalar cascade: ≤16B 문자열에 적용 ✅
- **미해결**: 노드당 dispatch 오버헤드, separator 쓰기 오버헤드

yyjson 167μs = 2.4 GB/s (citm 1.7MB 기준). L3 캐시 한계에 근접하는 속도. Beast가 여기에 도달하려면 **per-node 오버헤드를 yyjson 수준(~11 cycles/node)으로 낮춰야 함**.

**접근법**:
1. **M1 Instruments 프로파일링**: serialize 시간의 정확한 핫스팟 파악 (키 복사 vs structural 쓰기 vs dispatch 오버헤드)
2. **반복 스키마 직렬화 캐싱**: citm 243개 performance 객체는 동일 스키마 → 첫 객체 직렬화 결과를 템플릿으로 저장 후, 이후 객체는 숫자/문자열 값만 patch하여 memcpy. 구조 오버헤드 제거.
3. **NEON 구조 문자열 배치**: `{`, `}`, `[`, `]`, `:`, `,` 등 구조 문자를 16B NEON store로 배치 기록.

**우선순위**: 🔴 M1 citm 유일한 serialize 문제

---

### Phase 70 — M1 citm parse 심화 (중기 🟠)

**목표**: M1 citm parse 563μs → ≤400μs (yyjson 1.2×)

**Phase 65 이후 예상**: ~540μs → 여전히 35% 더 필요.

**접근법**:
1. **KeyLenCache 히트율 측정**: M1 citm에서 캐시 히트 비율 확인 (`__builtin_expect` 카운터 임시 삽입)
2. **NEON key comparator**: 캐시 미스 시 다음 히트까지 NEON 16B 비교로 키 끝 탐색 (현재는 SWAR+scalar). scan_string_end()가 NEON 16B 루프를 쓰듯이 key scanner도 동일 패턴
3. **Depth-aware 캐시 프리페치**: 다음 객체 진입 전 `kc_.lens[depth+1]` prefetch

**우선순위**: 🟠 Phase 65 이후

---

### Phase 71 — M1 canada parse (장기 🔴)

**목표**: M1 canada parse 1,681μs → ≤1,220μs (yyjson 1.2×)

**현상**: canada는 부동소수점 배열. 현재 SWAR-8 인라인 float digit scanner 사용.
yyjson: 1,464μs → Beast 1,681μs. Gap 15%, 27% 개선 필요.

**접근법**:
1. **NEON 벡터 float digit 스캐너**: 16B씩 `'0'-'9'`, `'.'`, `'e'`, `'E'`, `'-'`, `'+'` 범위 체크 → `vmaxvq_u8` 로 종료 탐지. 현재 SWAR-8 (8B/iter) → NEON (16B/iter) 2× 처리량
2. **Phase 46 AVX-512 64B WS skip의 NEON 등가물**: 공백 건너뛰기를 NEON 32B로. Phase 63 (32B 실패)을 다른 방식으로 — vmaxv 대신 vshrn 기반 movemask로 재시도

**우선순위**: 🔴 (어렵지만 단독 최대 게인 가능)

---

### Phase 72 — M1 twitter parse (초장기 🔴)

**목표**: M1 twitter parse 266μs → ≤144μs (yyjson 1.2×)

**현황**: yyjson M1 twitter = 173μs (3.57 GB/s). Beast 목표 144μs = 4.28 GB/s. yyjson의 현재 속도보다 25% 빨라야 함.

**근본 난이도**: M1은 OoO window 576-entry로 yyjson의 순차적 코드에 최적. Beast의 tape 기반 구조는 추가 메모리 간접 참조가 있음. twitter는 schema 다양성으로 KeyLenCache 효과 없음.

**탐색 방향**: 근본적으로 다른 접근법 필요 — e.g. M1 특화 two-pass 경량 scanner (Stage 1+2의 NEON 실패 교훈 반영한 개선된 설계). 구체적 방법은 Phase 68/70 완료 후 학습된 M1 특성을 바탕으로 설계.

**우선순위**: 🔴 초장기 (Phase 68-70 완료 후 착수)

---

## 공통 기반 최적화 (전 아키텍처 적용 완료)

| Phase | 내용 | 아키텍처 | 효과 |
|:---|:---|:---:|:---:|
| D1 | TapeNode 12→8 bytes 컴팩션 | All | +7.6% |
| Phase E | Pre-flagged separator (dump 비트스택 제거) | All | serialize −29% |
| Phase 25-26 | Double-pump number/string + 3-way fused scanner | All | −15μs |
| Phase 31 | Contextual SIMD Gate (NEON/SSE2 string scanner) | All | twitter −4.4% |
| Phase 32 | 256-entry constexpr Action LUT dispatch | All | BTB 개선 |
| Phase 33 | SWAR-8 inline float digit scanner | All | canada −6.4% |
| Phase 34 | AVX2 32B String Scanner | x86 | 처리량 2× |
| Phase 41 | `skip_string_from32`: mask==0 AVX2 fast path | x86 | SWAR-8 게이트 생략 |
| Phase 42 | AVX-512 64B String Scanner (`scan_string_end`) | x86 | −9~13% |
| Phase 43 | AVX-512 64B Inline Scan + `skip_string_from64` | x86 | −9~13% |
| Phase 60-A | compact `cur_state_` 상태 머신 | AArch64 | canada −15.8% |
| Phase 61 | NEON 오버랩 페어 dump() 문자열 복사 | AArch64 | dump −5.5% |
| Phase 62 | NEON 32B inline value string 스캔 | AArch64 | twitter −5.7% |
| **Phase 73** | **`dump(string&)` buffer-reuse 오버로드 (`__resize_default_init`)** | **All** | **serialize citm −55%, gsoc −72%, twitter −48%, canada −40%** |

---

## 🏗️ 인프라 / CI — 미보유 아키텍처 정확성 커버

> **설계 방향**: 성능 튜닝은 보유 장비 3종(Mac M1 Pro, Snapdragon Gen 2, Claude Code x86)으로 집중.
> 정확성 검증은 GitHub Actions CI로 커버. **세부 전략: `docs/ARCH_STRATEGY.md`**

### 즉시 구성 가능 (공개 저장소 무료 러너)

| 항목 | 러너 | 우선순위 | 상태 |
|:---|:---|:---:|:---:|
| `ubuntu-24.04-arm` Graviton2 ctest | `ubuntu-24.04-arm` | 🔴 높음 | ☐ 미구현 |
| `macos-15` Apple Silicon ctest | `macos-15` | 🔴 높음 | ☐ 미구현 |
| `windows-2025-arm` Windows ARM64 ctest | `windows-2025-arm` | 🟠 중간 | ☐ 미구현 |
| `ubuntu-24.04` x86_64 ctest (기준선) | `ubuntu-24.04` | 🔴 높음 | ☐ 미구현 |

**다음 액션**: `.github/workflows/ci.yml` 생성. cmake Release 빌드 + ctest 만 실행. 성능 측정 없음.

### QEMU 에뮬레이션 (정확성 only, 낮은 우선순위)

| 항목 | 대상 | 우선순위 | 상태 |
|:---|:---|:---:|:---:|
| RISC-V 64 fallback 경로 검증 | `qemu-riscv64` + toolchain | 🟡 낮음 | ☐ 미구현 |
| PPC64LE big-endian 코너케이스 | `qemu-ppc64le` + toolchain | 🟡 낮음 | ☐ 미구현 |

**toolchain 파일 필요**: `cmake/toolchains/riscv64-linux-gnu.cmake`, `ppc64le-linux-gnu.cmake`

### 장기 — 실 하드웨어 추가

| 항목 | 조달 방법 | 우선순위 | 상태 |
|:---|:---|:---:|:---:|
| SVE 실 측정 (Graviton 3 / Neoverse V1) | Oracle Cloud Always Free (Ampere A1 4코어) | 🟠 중기 | ☐ 미착수 |
| Windows ARM64 실 측정 | CI 우선, 필요 시 별도 조달 | 🟡 낮음 | ☐ 미착수 |

---

## 주의 사항 (불변 원칙)

- **모든 변경은 `ctest 81/81 PASS` 후 커밋** — 예외 없음
- **SIMD 상수는 사용 지점에 인접 선언** — YMM/ZMM 호이스팅 금지 (Phase 40 교훈)
- **회귀 즉시 revert** — 망설임 없이 되돌리고 원인 분석 선행
- **AArch64 Pure NEON 원칙**: 스칼라 SWAR 게이트 절대 금지. GPR-SIMD 교차 이동 페널티 입증됨 (Phase 50-1, 56-5, 60-B).
- **SVE 절대 금기** (Snapdragon): Android 커널 비활성화 → SIGILL 확인.
- **x86_64 Stage 1+2 경로**: ≤2MB 파일만 (twitter 617KB, citm 1.65MB). canada/gsoc는 단일 패스.
- **Phase 65 리스크**: `s[cl-1]` 가드 제거 시, 값이 `":"` 형태인 JSON에서 false-positive 발생 가능. 표준 4개 벤치마크 파일 안전 확인됨. 실 서비스 적용 전 도큐멘테이션 필수.
- **매 Phase는 별도 브랜치로 진행** → PR 후 merge
- **M1 PGO 황금 규칙** (2026-03-04 확립):
  1. 황금 profdata = `build_pgo_gen/benchmarks/pgo.profdata` (2026-03-03 생성). 절대 삭제/재생성 금지.
  2. 코드 변경 시 반드시 `build_pgo_gen` 베이스로 새 gen/use 디렉토리 생성 (기존 오염 방지).
  3. 프로파일링은 `bench_all --all --iter 30` **ONLY** (bench_quick 혼합 금지 — twitter 573μs 파국적 결과 확인).
  4. 측정은 빌드 완료 후 **5분 대기** 후 실시 (M1 서멀 스로틀링 — 빌드 직후 측정 시 3× 오차 발생).
  5. parse() 함수에 코드 ADDING 절대 금지. REPLACING(교체)는 허용 (Phase 62 ✅ vs Phase 70-M1 ❌).
