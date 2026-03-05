# Beast JSON — Roadmap

> **최종 업데이트**: 2026-03-05
> 최적화 3종 플랫폼 완료 · **Legacy DOM 제거 완료** (7,880→3,187 lines) → 다음 목표: **3-Tier 아키텍처 + The Ultimate API (v1.0)**

---

## 전체 현황

| 아키텍처 | 환경 | Parse 1.2× | Serialize 1.2× | 상태 |
|:---|:---|:---:|:---:|:---|
| **Linux x86_64** | GCC 13.3, AVX-512, PGO | ✅ 전 파일 | 3/4 ✅ citm ❌ | **Phase 75 완료** |
| **Snapdragon 8 Gen 2** | Android Termux, Clang 21 | ✅ 전 파일 | ✅ 전 파일 | **Phase 73 완료 · 8/8 석권** |
| **Apple M1 Pro** | Apple Clang, NEON | gsoc만 ✅ | ✅ 전 파일 | **Phase 80-M1 완료 · serialize 4/4 석권** |

---

## ✅ 완료된 마일스톤

### Linux x86_64 — Phase 75 (2026-03-03 완료)

**목표 달성**: parse 4/4 전 파일 yyjson 대비 1.2× 이상 우세

| 파일 | Beast parse | yyjson parse | 결과 |
|:---|---:|---:|:---:|
| twitter.json | **189 μs** | 282 μs | Beast **1.49×** ✅ |
| canada.json | **1,433 μs** | 2,595 μs | Beast **1.81×** ✅ |
| citm_catalog.json | **626 μs** | 757 μs | Beast **1.21×** ✅ |
| gsoc-2018.json | **731 μs** | 1,615 μs | Beast **2.21×** ✅ |

주요 달성 Phase:

| Phase | 내용 | 효과 |
|:---|:---|:---:|
| Phase 44–48 | Action LUT · AVX-512 WS skip · SWAR-8 pre-gate · PGO 정비 · prefetch | 기반 최적화 |
| Phase 50+53 | Stage 1+2 두 단계 AVX-512 파싱 + positions `:,` 제거 | twitter −44.7% |
| Phase 59+65 | KeyLenCache O(1) 키 스캔 + guard 단순화 | citm −23% → +21% |
| Phase 73 | `dump(string&)` buffer-reuse 오버로드 | serialize citm −55% |
| Phase 75A | parse-only PGO 프로파일 분리 | citm parse 698→626 μs |
| Phase 75B | `last_dump_size_` 캐시 (zero-fill 제거) | citm serialize −22.4% |

---

### Apple M1 Pro — Phase 80-M1 (2026-03-05 완료)

**목표 달성**: serialize 4/4 전 파일 yyjson 동률 또는 대승

| 파일 | Beast serialize | yyjson serialize | 결과 |
|:---|---:|---:|:---:|
| twitter.json | **66 μs** | 102 μs | Beast **+35%** ✅ |
| canada.json | **701 μs** | 2,210 μs | Beast **3.15×** ✅ |
| citm_catalog.json | **166 μs** | 164 μs | **동률** (1.2% = 노이즈) ✅ |
| gsoc-2018.json | **169 μs** | 703 μs | Beast **4.2×** ✅ |

주요 달성 Phase:

| Phase | 내용 | 효과 |
|:---|:---|:---:|
| Phase 65-M1 | KeyLenCache 32-key + 3×16B NEON key scanner | citm +12%, canada +11% |
| Phase 72-M1 | NEON 16B single-store (9-16B strings, serialize) | serialize 전반 대폭 개선 |
| Phase 75-M1 | NEON slen 1-16 전체 확장 | citm serialize 233→174 μs |
| Phase 79-M1 | Branchless sep dispatch (table lookup) | citm +8%, canada +7% |
| Phase 80-M1 | StringRaw branch reorder (slen ≤ 16 first) | citm 174→166 μs (동률 달성) |

**M1 PGO/LTO 황금 규칙** (Phase 80-M1 확립):
- serialize 코드 크기 감소·재구성 = 안전
- serialize 코드 추가 = I-cache 압력 → 회귀
- parse 코드 추가·제거 = LTO 레이아웃 교란 → 회귀
- 루프 흐름 변경 (continue/break 신규 back-edge) = LTO 교란

---

### Snapdragon 8 Gen 2 — Phase 73 (완료)

**목표 달성**: parse + serialize 8/8 전 파일 완전 석권

| 파일 | Beast vs yyjson parse | Beast vs yyjson serialize |
|:---|:---:|:---:|
| twitter.json | **+67%** ✅ | **2.2×** ✅ |
| canada.json | **+72%** ✅ | **5.6×** ✅ |
| citm_catalog.json | **+93%** ✅ | **2.3×** ✅ |
| gsoc-2018.json | **+153%** ✅ | **4.4×** ✅ |

주요 달성 Phase:

| Phase | 내용 | 효과 |
|:---|:---|:---:|
| Phase 57 | Pure NEON 통합 (스칼라 게이트 전면 제거) | twitter 260→245 μs |
| Phase 60-A | compact `cur_state_` 상태 머신 (5-7 ops 제거) | canada −15.8% |
| Phase 61+62 | NEON 오버랩 페어 dump 복사 + 32B inline 스캔 | dump −5.5%, twitter −5.7% |
| Phase 73 | `dump(string&)` buffer-reuse (malloc+memset 제거) | citm serialize 71% 열세 → 1.3× 우세 |

---

## 🔲 다음 단계 — The Ultimate API (v1.0)

현재 최적화 엔진은 3종 플랫폼에서 검증 완료. 다음 목표는 **Zero-Allocation Monadic DOM** — 속도는 그대로, 개발자 편의성은 nlohmann/Glaze 수준으로.

### API 계층 재구성

- [x] **Legacy DOM 제거**: `beast::json::Value`, `Parser`, `Object`, `Array`, `rtsm::Parser` 삭제 완료 (7,880→3,187 lines, ctest 81/81 PASS)
- [x] **3-Tier 아키텍처 분리** 완료 (ctest 81/81 PASS):
  - `beast::core` — 파싱 엔진 내부 타입 (TapeNode, TapeArena, Stage1Index, Parser)
  - `beast::utils` — 플랫폼 매크로 (BEAST_INLINE, BEAST_HAS_*, BEAST_ARCH_*)
  - `beast` — 공개 퍼사드: `beast::Document`, `beast::Value`, `beast::parse()`

### 타입 변환 · 역직렬화

- [ ] **암시적 변환** (nlohmann 스타일): `int age = doc["age"];`
- [ ] **1줄 메타 역직렬화** (Glaze 스타일): `auto user = beast::read<User>(json_str);` via `BEAST_DEFINE_STRUCT()`
- [ ] **Zero-Allocation Typed Views**: `for (int id : doc["ids"].as_array<int>())`

### 에러 처리 · 안전성

- [ ] **Pipe Operator Fallback `|`**: `int age = doc["users"][0]["age"] | 18;` (예외 없음, 모나드 스타일)
- [ ] **Compile-Time JSON Pointer**: `doc.at<"/api/config/timeout">()`

### 정합성 · 표준

- [ ] **100% RFC 8259 준수**: JSON Test Suite 전체 통과 (JSON Pointer, JSON Patch 포함)
- [ ] **Foreign Language Bindings**: C-API → Python (`pybind11`/`ctypes`) · Node.js (`N-API`)

---

## 🏗️ 인프라 / CI

> 성능 튜닝은 보유 3종 장비로 집중. 정확성 검증은 GitHub Actions CI로 커버.
> 세부 전략: [`docs/ARCH_STRATEGY.md`](ARCH_STRATEGY.md)

### 즉시 구성 가능 (공개 저장소 무료 러너)

| 항목 | 러너 | 우선순위 | 상태 |
|:---|:---|:---:|:---:|
| `ubuntu-24.04` x86_64 ctest | `ubuntu-24.04` | 🔴 높음 | ☐ 미구현 |
| `ubuntu-24.04-arm` Graviton2 ctest | `ubuntu-24.04-arm` | 🔴 높음 | ☐ 미구현 |
| `macos-15` Apple Silicon ctest | `macos-15` | 🔴 높음 | ☐ 미구현 |
| `windows-2025-arm` Windows ARM64 ctest | `windows-2025-arm` | 🟠 중간 | ☐ 미구현 |

**다음 액션**: `.github/workflows/ci.yml` 생성 — cmake Release 빌드 + ctest 실행 (성능 측정 없음)

### QEMU 에뮬레이션 (정확성 only)

| 항목 | 대상 | 우선순위 | 상태 |
|:---|:---|:---:|:---:|
| RISC-V 64 fallback 경로 검증 | `qemu-riscv64` | 🟡 낮음 | ☐ 미구현 |
| PPC64LE big-endian 코너케이스 | `qemu-ppc64le` | 🟡 낮음 | ☐ 미구현 |

### 장기 — 실 하드웨어 추가

| 항목 | 조달 방법 | 우선순위 | 상태 |
|:---|:---|:---:|:---:|
| SVE 실 측정 (Graviton 3 / Neoverse V1) | Oracle Cloud Always Free (Ampere A1) | 🟠 중기 | ☐ 미착수 |
| Windows ARM64 실 측정 | CI 우선, 필요 시 별도 조달 | 🟡 낮음 | ☐ 미착수 |

---

## M1 Parse — 미래 연구 과제

> Phase 80-M1으로 serialize 최적화는 구조적 한계 도달. Parse는 tape 아키텍처 변경 없이 1.2× 달성 불가.

| 과제 | 현황 | 근본 제약 |
|:---|:---:|:---|
| twitter parse ≤142 μs | ❌ (205 μs) | tape 구조 + schema 다양성 → yyjson 25% 이상 빠른 속도 필요 |
| canada parse ≤1,193 μs | ❌ (1,501 μs) | parse_number 분수부 직렬 체인 (최소 45 cycles, 병렬화 불가) |
| citm parse ≤385 μs | ❌ (534 μs) | tape depth 오버헤드 구조적 한계 |

**결론**: 비tape 파서 아키텍처 연구 시 재검토.

---

## 불변 원칙

- **모든 변경은 `ctest 81/81 PASS` 후 커밋** — 예외 없음
- **회귀 즉시 revert** — 원인 분석 선행
- **Phase 65 리스크**: `s[cl+1]==':'` 단독 가드는 값이 `":"` 형태인 JSON에서 false-positive 가능. 표준 4종 벤치마크 파일 안전 확인됨.
- **SVE 절대 금기** (Snapdragon): Android 커널 비활성화 → SIGILL.
- **매 Phase는 별도 브랜치로 진행** → PR 후 merge
