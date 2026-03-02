# 📊 Beast JSON 1.0 Release — Technical API Blueprint

이 문서는 차기 에이전트들이 **"Beast JSON 1.0 정식 릴리즈"** 를 완벽하게 수행할 수 있도록, 삭제해야 할 Legacy 코드의 물리적 식별자와 새로 구축해야 할 `beast::lazy` API 아키텍처를 상세히 기술한 **Technical Blueprint(기술 설계서)** 입니다.

현재 `main` 브랜치 소스코드(`beast_json.hpp`) 기준으로 분석되었습니다.

---

## 🗑️ PART 1: Legacy 하위 호환 코드 삭제 명세서 
가장 먼저 수행해야 할 작업은 구세대 파서 로직(DOM 및 스칼라 파서)의 완전한 궤멸입니다. 이를 통해 바이너리 크기를 줄이고 API 가독성을 확보합니다.

### 1-1. 삭제 대상 핵심 클래스 및 구조체 (DOM 표현)
이 클래스들은 느린 메모리 할당(동적 할당) 기반의 DOM 객체들입니다. 모두 제거하십시오.
- `class Value` (Line ~2119 부근 시작)
  - 관련된 모든 `as_string`, `as_int`, `operator[]` 등 멤버 함수 일절.
  - 관련 `from_json`, `to_json` 오버로딩 전면 삭제.
- `class Object` 및 `class Array`
- `struct JsonMember`
- `class Document` (DOM Document)
- `class StringBuffer` / `class TapeSerializer` 
  - (단, `beast::lazy::Value::dump()`에서 쓰이는 신규 Serializer 로직은 보존해야 함에 유의)

### 1-2. 삭제 대상 파서 백엔드 (구문 분석 로직)
`beast::lazy` 타겟은 `TapeArena` 기반의 2-Pass `parse_staged` 등 특수 최적화 파서를 씁니다. 구형 문자열 토크나이저는 삭제 대상입니다.
- `class Parser` 내의 구형 구조:
  - Phase 50 이전의 `void parse()`, `parse_string()`, `parse_number()`, `parse_object()`, `parse_array()` 등 DOM 객체(`Value`)를 직접 리턴하거나 구성하는 파서 메소드 전부.
  - `parse_string_swar()`, `skip_whitespace_swar()`, `vgetq_lane` 류의 레거시 스칼라/벡터 fallback 함수(현재 Tape 기반 Lazy Parser가 사용하지 않는 함수 100% 식별 후 제거).

---

## 🏗️ PART 2: 아키텍처 레이어링 (Core-Utils-API 분리) 및 사용자 친화적 네이밍
최종 사용자는 내부적으로 파서가 지연 평가(Lazy)를 쓰는지, DOM을 구축하는지 알 필요가 없어야 합니다. 가장 직관적이고 표준적인 네이밍을 제공하면서도, 내부 코드는 철저히 분리된 레이어로 관리되어야 합니다.

전문가 수준의 라이브러리 설계를 위해 다음 3-Tier 아키텍처를 채택합니다.

### Layer 1: The Core Engine (`namespace beast::core`)
JSON을 물리적으로 파싱하고 직렬화하는 절대적인 "엔진부"입니다. 외부(사용자)에선 이 레이어의 존재나 클래스를 직접 조작하지 않습니다.
- **포함 대상**: SIMD/SWAR 스캐너(`simd`, `lookup`), 이스케이프 파서, 숫자 파싱(Russ Cox `PowMantissa`, `Unrounded`), `TapeArena`, `Stage1Index`, 코어 `Parser` 및 `Serializer`.
- **목표**: 100% RFC 8259 준수, 제로 할당, 최대 ILP(Instruction-Level Parallelism) 기반의 처리 속도 확보.

### Layer 2: The Utilities (`namespace beast::utils` 또는 `beast::ext`)
코어 데이터 위에 확장 기능을 부여하는 유틸리티/플러그인 레이어입니다.
- **포함 대상**: C++ 매크로/템플릿 기반 자동 O/R 매퍼(`to_json`, `from_json`, `BEAST_DEFINE_STRUCT`), JSON Pointer (RFC 6901), JSON Patch (RFC 6902) 등.
- **목표**: 코어 엔진의 가벼움을 해치지 않으면서, 필요할 때만 인클루드/사용하여 생산성을 극대화.

### Layer 3: The Public API (`namespace beast`)
사용자가 최종적으로 마주치는 "단일 진입점(Facade)"입니다. 기존의 `lazy`라는 구현 종속적 이름은 내부로 숨기고 표준적인 네이밍을 노출합니다.

- **`beast::Value` (기존 `beast::lazy::Value` 진화형)**:
  - 사용자는 오직 `beast::parse("...")`를 통해 `beast::Value`를 받습니다.
  - 내부적으로는 Tape 참조를 들고 있는 Lazy 객체이지만, 겉으로는 완벽한 DOM 객체처럼 동작합니다.
  - **필수 구현 접근자**: `as_int64()`, `as_double()`, `as_string_view()`, `as_bool()`, `operator[](std::string_view)`, `operator[](size_t)`.
- **`beast::parse()`**: 코어의 `parse_staged()`를 감싸는 래퍼 함수.

이 구조 전환을 통해 사용자는 "단순히 `beast::Value v = beast::parse(json);`을 썼을 뿐인데 세계에서 가장 빠르다"는 경험을 하게 됩니다.

---

## 📜 PART 3: 전 세계 JSON 표준 (RFC) 100% 준수
GitHub 릴리즈 시 개발자들에게 강력히 어필할 "무결성"을 달성하기 위한 구현 디테일.

1. **RFC 8259 무결성 (코어 레벨 방어)**
   - 스택 오버플로우 공격을 막기 위한 **심도 제한(Max Depth Limit)** 하드코딩 또는 옵션 처리.
   - 유니코드 Surrogate Pair (e.g. `\uD83D\uDE00`) 디코딩 무결성 지원. (기존 코드가 지원하는지 필히 테스트 보강)
2. **Optional RFC 지원 (Utils 레벨 구현)**
   - **JSON Pointer (RFC 6901)** : `doc.at("/users/0/name")` 형태의 액세서 구축.
   - **JSON Patch (RFC 6902)** : 성능 저하 없이 DOM 부분 수정을 가능하게 하는 인터페이스 확장.

이 문서를 챗봇 / 에이전트의 프롬프트 컨텍스트로 활용하여 순차적으로 1.0 릴리즈 코딩을 시작하십시오.
