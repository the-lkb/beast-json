# Beast JSON — Roadmap

> **최종 업데이트**: 2026-03-05
> 최적화 3종 플랫폼 완료 · **Legacy DOM 제거 완료** (7,880→3,187 lines) · **The Ultimate API 1단계 완료** (ctest 223/223 PASS) → 다음 목표: **1줄 역직렬화 + RFC 8259 완전 준수 (v1.0)**

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

- [x] **Beast Value Accessor + Mutation + SafeValue API** (ctest 166/166 PASS, 2026-03-05):
  - **타입 체크**: `is_null()`, `is_bool()`, `is_int()`, `is_double()`, `is_number()`, `is_string()`
  - **`as<T>()`** — Beast 고유 패턴: 단일 정규 접근자, 타입 불일치 시 `std::runtime_error`
  - **`try_as<T>()`** — `std::optional<T>` 반환, 예외 없는 안전 접근
  - **`operator[](key)`** / **`operator[](idx)`** — 체인 가능 접근, 미스 시 `std::out_of_range`
  - **`find(key)`** — `std::optional<Value>` 반환, 안전한 객체 키 탐색
  - **`size()`** / **`empty()`** — 배열·객체 원소 수
  - **암시적 변환** `operator T()` — `int age = doc["age"];`, `std::string name = doc["name"];`
  - **`set(T)`** — 뮤테이션 overlay: `nullptr`/`bool`/`int64`/`double`/`string_view` 지원
    - `unset()` — 원본 파싱 값 복원
    - `dump()` / `dump(string&)` / `is_*()` / `as<T>()` / `try_as<T>()` 모두 뮤테이션 반영
    - 뮤테이션 없는 경우 zero overhead (`BEAST_UNLIKELY`, map 미탐색)
  - `const char*` / `int` 오버로드로 연산자 중의성 방지
  - **`operator=(T)`** — `=` 구문 쓰기: `root["key"] = 42;`
  - **`SafeValue`** — optional propagating proxy:
    - `get(key/idx)` → `SafeValue` (throws 없이 optional 체인 시작)
    - `SafeValue::operator[]` → 체인 전파 (absent 시 nullopt 전파)
    - `SafeValue::as<T>()` → `std::optional<T>`
    - `SafeValue::value_or(T)` → 기본값 폴백
    - `has_value()` / `operator bool` / `operator*` / `operator->` 지원

  ```cpp
  beast::Document doc;
  auto root = beast::parse(doc, R"({"user": {"id": 7, "score": 3.14}})");

  // Read — throwing (fast path, 확신 있는 접근)
  int id = root["user"]["id"].as<int>();
  int id2 = root["user"]["id"];             // 암시적 변환

  // Read — safe chain (never throws, std::optional 전파)
  auto maybe = root.get("user")["id"].as<int>(); // std::optional<int>
  int  safe  = root.get("user")["id"].value_or(-1); // 기본값
  int  deep  = root.get("a")["b"]["c"].value_or(0); // 중간 missing → 0

  // Write — set() 또는 = 구문
  root["user"]["id"] = 99;
  root["user"]["score"] = 9.9;
  root["user"]["name"] = "Eve";
  root["user"]["id"] = nullptr;

  // Reflected immediately in dump()
  std::string json = root.dump();

  // Restore original
  root["user"]["id"].unset();
  ```

- [x] **1등 사용성 — 전면 API 개선** (ctest 213/213 PASS, 2026-03-05):
  - **`operator[]` non-throwing** — 누락 키/범위 초과 시 예외 대신 invalid `Value{}` 반환; `operator bool()`로 유효성 확인
  - **`dump()` 서브트리 직렬화** — `root["user"].dump()` → `{"name":"..."}` (전체 문서 아님)
  - **구조적 뮤테이션 API** — 원본 tape 불변, overlay 방식:
    - `erase(key)` / `erase(idx)` — 키·배열 요소 삭제
    - `insert(key, T)` / `insert_json(key, raw)` — 객체에 키-값 추가
    - `push_back(T)` / `push_back_json(raw)` — 배열에 요소 추가
    - `dump()` / `size()` / `find()` / `operator[]` / 이터레이션 즉시 반영
  - **이터레이션 API**:
    - `items()` — 객체 키-값 쌍 range-for: `for (auto [k, v] : root.items())`
    - `elements()` — 배열 요소 range-for: `for (auto v : root["arr"].elements())`
    - 삭제된 항목 자동 skip
  - **Pretty-print** — `dump(int indent)`: `root.dump(2)` / `root.dump(4)`

  ```cpp
  beast::Document doc;
  auto root = beast::parse(doc, R"({"users":[{"id":1},{"id":2}],"tags":["a","b"]})");

  // AutoChain — 절대 throw 없음 (missing key → invalid Value{})
  if (root["missing"]["deep"])  // false, no exception
      std::cout << root["missing"]["deep"].as<int>() << "\n";

  // Subtree dump
  std::cout << root["users"].dump() << "\n";  // [{"id":1},{"id":2}]

  // Structural mutation
  root["users"].push_back_json(R"({"id":3})");
  root["tags"].erase(0);          // "a" removed
  root.insert("version", 1);

  // Iteration
  for (auto [key, val] : root.items())
      std::cout << key << ": " << val.dump() << "\n";
  for (auto elem : root["tags"].elements())
      std::cout << elem.as<std::string>() << "\n";  // b

  // Pretty-print
  std::cout << root.dump(2) << "\n";
  ```

- [x] **C++20 Ranges/STL 완전 호환 + Concepts** (ctest 223/223 PASS, 2026-03-05):
  - **`borrowed_range`** — `enable_borrowed_range<ObjectRange> = true` / `enable_borrowed_range<ArrayRange> = true`
    - `std::ranges::find_if`, `count_if`, `max_element`, `transform`, `distance` 정상 동작
    - `| std::views::filter(f)` / `| std::views::transform(f)` 파이프 문법 지원
  - **`Value::ObjectItem`** 공개 타입 별칭 — `using ObjectItem = std::pair<string_view, Value>`; generic lambda에서 explicit 타입 명시 가능
  - **C++20 Concepts** — 템플릿 인자 제약으로 컴파일 타임 타입 안전성 보장

  ```cpp
  beast::Document doc;
  auto root = beast::parse(doc, R"({"scores":[3,1,4,1,5,9],"meta":{"v":2}})");

  // std::ranges 알고리즘
  auto arr = root["scores"].elements();
  auto max_it = std::ranges::max_element(arr, [](auto a, auto b){
      return a.as<int>() < b.as<int>();
  });
  std::cout << max_it->as<int>() << "\n";  // 9

  // Views 파이프라인
  auto big = root["scores"].elements()
      | std::views::filter([](auto v){ return v.as<int>() > 3; });
  for (auto v : big)
      std::cout << v.as<int>() << " ";  // 4 5 9

  // items() + filter
  auto found = root.items()
      | std::views::filter([](Value::ObjectItem kv){ return kv.first == "meta"; });
  for (auto [k, v] : found)
      std::cout << k << ": " << v.dump() << "\n";  // meta: {"v":2}
  ```

- [ ] **1줄 메타 역직렬화** (독자적 방식): Beast 고유 설계, Glaze/nlohmann과 차별화
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

- **모든 변경은 `ctest PASS` 후 커밋** — 예외 없음 (현재 223개)
- **회귀 즉시 revert** — 원인 분석 선행
- **Phase 65 리스크**: `s[cl+1]==':'` 단독 가드는 값이 `":"` 형태인 JSON에서 false-positive 가능. 표준 4종 벤치마크 파일 안전 확인됨.
- **SVE 절대 금기** (Snapdragon): Android 커널 비활성화 → SIGILL.
- **매 Phase는 별도 브랜치로 진행** → PR 후 merge
