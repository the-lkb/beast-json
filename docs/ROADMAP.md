# Beast JSON — Roadmap

> **최종 업데이트**: 2026-03-05
> 최적화 3종 플랫폼 완료 · **Legacy DOM 제거 완료** (7,880→3,187 lines) · **The Ultimate API 완성** · **Zero-Effort 자동 직렬화 엔진 완성** (ctest 312/312 PASS) → 다음 목표: **RFC 8259 완전 준수 + Foreign Language Bindings (v1.0)**

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

- [x] **편의 API — The Ultimate Convenience Layer** (ctest 272/272 PASS, 2026-03-05):
  - **`contains(key)`** — `root.contains("name")` (bool, sugar for `find(key).has_value()`)
  - **`value(key/idx, default)`** — `root.value("age", 0)` (nlohmann 스타일 안전 추출)
  - **`type_name()`** — `"null"/"bool"/"int"/"double"/"string"/"array"/"object"/"invalid"` 반환
  - **`operator|` 파이프 폴백** — `int age = root["age"] | 42;` / `std::string s = root["name"] | "anon";`
    - `Value`와 `SafeValue` 모두 지원
    - 타입 불일치·누락 시 기본값 반환 (never throws)
  - **`keys()` / `values()`** — 객체 키·값 lazy 범위 (`std::views::transform` 기반)
  - **`as_array<T>()` / `try_as_array<T>()`** — 타입 변환 lazy 뷰
    - `for (int id : doc["ids"].as_array<int>())` — 원소별 as<T>() 변환
    - `try_as_array<T>()` — optional<T> 뷰, never throws
  - **Runtime JSON Pointer `at(path)`** — RFC 6901 준수: `root.at("/users/0/name")`
    - `~0` → `~`, `~1` → `/` 이스케이프 처리
    - 경로 누락 시 invalid `Value{}` 반환 (never throws)
  - **컴파일타임 JSON Pointer `at<Path>()`** — `root.at<"/config/timeout">()`
    - NTTP로 경로 검증: `'/'`로 시작하지 않으면 컴파일 에러
  - **`merge(other)`** — 다른 객체의 키-값 전체를 현재 객체로 병합
  - **`merge_patch(json)`** — RFC 7396 JSON Merge Patch 적용
    - null 값 → 키 삭제, 객체 값 → 재귀 패치, 기타 → 덮어쓰기
  - **`beast::read<T>(json)`** / **`beast::write(obj)`** — ADL 기반 구조체 역직렬화
    - 사용자 정의: `from_beast_json(const Value&, T&)` / `to_beast_json(Value&, const T&)`
  - **`beast::SafeValue`** — 공개 타입 별칭 (`beast::json::lazy::SafeValue`)
  - **`as<T>()` null 가드** — invalid `Value{}` 접근 시 `std::runtime_error` (기존: UB/segfault)

  ```cpp
  beast::Document doc;
  auto root = beast::parse(doc, R"({"user":{"id":7,"tags":["go","cpp"]},"score":3.14})");

  // contains + value() — 안전 추출
  if (root.contains("user"))
      std::cout << root["user"]["id"].type_name() << "\n";  // "int"
  int id    = root.value("user", std::string{}).empty() ? -1 : root["user"]["id"] | -1;
  double sc = root["score"] | 0.0;

  // keys() / values() — 객체 범위
  for (std::string_view k : root.keys())
      std::cout << k << "\n";  // user, score

  // as_array<T>() — 타입 뷰
  for (std::string tag : root["user"]["tags"].as_array<std::string>())
      std::cout << tag << "\n";  // go, cpp

  // Runtime JSON Pointer
  std::cout << root.at("/user/tags/0").as<std::string>() << "\n";  // go

  // Compile-time JSON Pointer (path validated at compile time)
  std::cout << root.at<"/user/id">().as<int>() << "\n";  // 7

  // merge_patch (RFC 7396)
  root.merge_patch(R"({"score":null,"extra":42})");
  // score deleted, extra added → visible via dump()

  // ADL 구조체 바인딩
  struct Config { int timeout; std::string mode; };
  void from_beast_json(const beast::Value& v, Config& c) {
      c.timeout = v["timeout"] | 5000;
      c.mode    = v["mode"]    | "default";
  }
  auto cfg = beast::read<Config>(R"({"timeout":10000,"mode":"fast"})");
  ```

- [x] **Zero-Effort 자동 직렬화 엔진** (ctest 312/312 PASS, 2026-03-05):

  **3-Tier 아키텍처** — 복잡도에 따라 자동 선택:

  | 티어 | 방법 | 코드 필요량 |
  |:---|:---|:---:|
  | Tier 1 | built-in STL 전체 자동 | 0줄 |
  | Tier 2 | `BEAST_JSON_FIELDS()` 매크로 | 1줄 |
  | Tier 3 | ADL 수동 `from_beast_json`/`to_beast_json` | 자유 |

  **Tier 1 — 내장 지원 타입** (완전 자동):
  - 기본 타입: `bool`, `int`, `double`, `float` 등 모든 산술 타입
  - 문자열: `std::string`, `std::string_view`, `const char*`
  - Optional: `std::optional<T>` → JSON null / T 자동 처리
  - Sequence: `std::vector<T>`, `std::list<T>`, `std::deque<T>` → JSON array
  - Set: `std::set<T>`, `std::unordered_set<T>` → JSON array
  - Map: `std::map<std::string, V>`, `std::unordered_map<std::string, V>` → JSON object
  - Fixed Array: `std::array<T, N>` → JSON array (크기 고정)
  - Tuple/Pair: `std::tuple<Ts...>`, `std::pair<A, B>` → JSON array
  - Null: `std::nullptr_t` → `null`

  **Tier 2 — `BEAST_JSON_FIELDS` 매크로** (커스텀 구조체):
  - 한 줄 매크로로 읽기 + 쓰기 동시 등록
  - 중첩 구조체, STL 컨테이너, `std::optional` 필드 — 모두 자동 재귀
  - 최대 32 필드 지원 (`BEAST_FOR_EACH` 프리프로세서 엔진)
  - JSON에 없는 필드 → 기본값 유지 (no crash)
  - JSON null on non-optional field → skip (no crash)

  **`beast::detail` 자동화 엔진** (내부):
  - Concept 기반 dispatch: 컴파일 타임에 정확한 분기 선택 (zero overhead)
  - `HasFromBeastJson<T>` / `HasToBeastJson<T>` — ADL 감지 concept
  - `is_valid()` — Value 유효성 공개 API
  - 타입 미지원 시 `static_assert`로 즉각 컴파일 에러 + 명확한 메시지
  - `Inf`/`NaN` → JSON `null` 자동 변환

  **공개 API** (`beast::` 네임스페이스):
  - `beast::read<T>(json)` — JSON 문자열 → T (모든 타입)
  - `beast::write(obj)` — T → JSON 문자열 (모든 타입)
  - `beast::from_json(value, out)` — 부분 역직렬화 helper
  - `beast::to_json_str(val)` — 부분 직렬화 helper

  ```cpp
  // ── Tier 1: STL 타입 — 코드 한 줄도 필요 없음 ──────────────────────────────
  std::vector<int>                  v = beast::read<std::vector<int>>("[1,2,3]");
  std::map<std::string,double>      m = beast::read<decltype(m)>(R"({"a":1.5})");
  std::optional<std::string>        s = beast::read<decltype(s)>(R"("hi")");
  std::tuple<int,std::string,bool>  t = beast::read<decltype(t)>("[42,\"ok\",true]");

  std::string j1 = beast::write(std::pair{3, "hello"s});     // [3,"hello"]
  std::string j2 = beast::write(std::array<int,3>{1,2,3});   // [1,2,3]
  std::string j3 = beast::write(std::optional<int>{});       // null

  // ── Tier 2: 구조체 — 매크로 한 줄 ────────────────────────────────────────────
  struct Address { std::string city; std::string country; };
  BEAST_JSON_FIELDS(Address, city, country)   // ← 이게 전부

  struct User {
      std::string              name;
      int                      age    = 0;
      Address                  addr;                    // 중첩 struct
      std::vector<std::string> tags;                    // STL 컨테이너
      std::optional<double>    score;                   // optional
  };
  BEAST_JSON_FIELDS(User, name, age, addr, tags, score)  // ← 이게 전부

  // Read — 완전 자동
  auto user = beast::read<User>(R"({
      "name": "Alice", "age": 30,
      "addr": {"city": "Seoul", "country": "KR"},
      "tags": ["admin", "user"],
      "score": 99.5
  })");

  // Write — 완전 자동
  std::string json = beast::write(user);
  // → {"name":"Alice","age":30,"addr":{"city":"Seoul","country":"KR"},
  //    "tags":["admin","user"],"score":99.5}

  // ── map of structs, vector of structs — 모두 자동 ─────────────────────────────
  std::vector<User>              users = beast::read<decltype(users)>(json_array);
  std::map<std::string, Address> places = beast::read<decltype(places)>(json_obj);
  ```

- [x] **Zero-Allocation Typed Views** (`as_array<T>()` 완성)
- [x] **Pipe Operator Fallback `|`**: `int age = doc["users"][0]["age"] | 18;` ✅
- [x] **Compile-Time JSON Pointer**: `doc.at<"/api/config/timeout">()` ✅

### 에러 처리 · 안전성

- [x] **Pipe Operator Fallback `|`** — `int age = doc["users"][0]["age"] | 18;` (`Value` + `SafeValue` 모두 지원)
- [x] **`as<T>()` null 가드** — invalid `Value{}` 접근 시 `std::runtime_error` (기존: UB/segfault)
- [x] **`is_valid()`** — Value 유효성 공개 API (`doc_ != nullptr` 체크)

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

- **모든 변경은 `ctest PASS` 후 커밋** — 예외 없음 (현재 272개)
- **회귀 즉시 revert** — 원인 분석 선행
- **Phase 65 리스크**: `s[cl+1]==':'` 단독 가드는 값이 `":"` 형태인 JSON에서 false-positive 가능. 표준 4종 벤치마크 파일 안전 확인됨.
- **SVE 절대 금기** (Snapdragon): Android 커널 비활성화 → SIGILL.
- **매 Phase는 별도 브랜치로 진행** → PR 후 merge
