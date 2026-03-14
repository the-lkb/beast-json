# 📊 qbuem-json 1.0 Release — Technical API Blueprint

This document is a detailed **Technical Blueprint** designed to guide future agents in flawlessly executing the **"qbuem-json 1.0 Official Release."** It specifies the physical identifiers of the Legacy code that must be deleted, and outlines the new `qbuem` API architecture that must be built.

It has been analyzed based on the current source code (`qbuem_json.hpp`) in the `main` branch.

---

## 🗑️ PART 1: Legacy Backward-Compatibility Code Deletion Specification
The very first task is the complete annihilation of the older generation parser logic (DOM and scalar parsers). This will shrink the binary size and secure API readability.

### 1-1. Target Core Classes and Structs for Deletion (DOM Representation)
These classes are DOM objects based on slow dynamic memory allocation. Remove all of them.
- `class Value` (Starting around Line ~2119)
  - All related member functions such as `as_string`, `as_int`, `operator[]`, etc.
  - All related `from_json`, `to_json` overloads must be completely deleted.
- `class Object` and `class Array`
- `struct JsonMember`
- `class Document` (DOM Document)
- `class StringBuffer` / `class TapeSerializer` 
  - (Note: The new Serializer logic used inside `qbuem::Value::dump()` must be preserved)

### 1-2. Target Parser Backends for Deletion (Syntax Analysis Logic)
The target for `qbuem` uses highly optimized parsers like the `TapeArena`-based 2-Pass `parse_staged`. The older string tokenizers are targets for deletion.
- Older structures inside `class Parser`:
  - All parser methods prior to Phase 50 that directly return or construct DOM objects (`Value`), such as `void parse()`, `parse_string()`, `parse_number()`, `parse_object()`, `parse_array()`.
  - Legacy scalar/vector fallback functions like `parse_string_swar()`, `skip_whitespace_swar()`, `vgetq_lane` variants (Identify and remove 100% of the functions currently NOT used by the Tape-based DOM Parser).

---

## 🏗️ PART 2: Architecture Layering (Core-Utils-API Separation)
End-users should not need to know whether the parser uses DOM-based evaluation or builds a DOM internally. We must provide the most intuitive and standardized naming while keeping the internal code strictly separated into layers.

We adopt the following 3-Tier Architecture for expert-level library design.

### Layer 1: The Core Engine (`namespace qbuem::core`)
The absolute "engine" that physically parses and serializes JSON. The outside world (users) will not interact with implementations or classes in this layer directly.
- **Includes**: SIMD/SWAR scanners (`simd`, `lookup`), escape parsers, number parsing (Russ Cox `PowMantissa`, `Unrounded`), `TapeArena`, `Stage1Index`, core `Parser` and `Serializer`.
- **Goals**: 100% RFC 8259 compliance, zero-allocation, maximum processing speed via ILP (Instruction-Level Parallelism).

### Layer 2: The Utilities (`namespace qbuem::utils` or `qbuem::ext`)
A utility/plugin layer that adds extended functionality on top of the core data.
- **Includes**: C++ macro/template-based automatic O/R mappers (`to_json`, `from_json`, `QBUEM_JSON_FIELDS`), JSON Pointer (RFC 6901), JSON Patch (RFC 6902), etc.
- **Goals**: Maximize productivity by allowing inclusion/usage only when necessary, without compromising the lightweight nature of the core engine.

### Layer 3: The Public API (`namespace qbuem`)
The single "Facade" entry point that the user ultimately encounters. The implementation-dependent namespace `DOM` is hidden internally, exposing standard nomenclature.

- **`qbuem::Value` (Evolution of the former `qbuem::Value`)**:
  - Users only receive `qbuem::Value` through `qbuem::parse("...")`.
  - Internally, it is a DOM object holding a Tape reference, but externally it behaves perfectly like a conventional DOM object.
  - **Required Accessors**: `as_int64()`, `as_double()`, `as_string_view()`, `as_bool()`, `operator[](std::string_view)`, `operator[](size_t)`.
- **`qbuem::parse()`**: A wrapper function around the core's `parse_staged()`.

### Layer 4: Modern Error Handling (`std::optional`) & Fluent Chaining
Instead of throwing complex exceptions or returning arcane error codes, we leverage C++17's `std::optional` to build a **"safe and intuitive error handling"** system.

- **Safe Data Extraction (Safe Accessors)**:
  - `std::optional<int64_t> get_int64() const` 
  - `std::optional<std::string_view> get_string() const`
  - If the value does not exist or the type mismatches, it returns `std::nullopt`, allowing users to elegantly handle errors like `if (auto val = obj["key"].get_int64())`.

- **World's Best Extreme Usability**:
  - **1) Fluent Chaining (Throw-less Deep Traversal)**: When doing deep traversal like `doc["users"][0]["name"]`, if "users" is not an array or the "name" key is missing halfway, the program will not crash or throw exceptions. Instead, it propagates a `Null/Error Value` state internally, allowing a safe single-step fallback like `.get_string().value_or("Unknown")` at the very end via Monadic patterns.
  - **2) Default Value Getters**: Provides intuitive APIs to immediately return a default value if missing or type mismatched, e.g., `doc["age"].get_int64(18)`.
  - **3) Range-based For Loop & Structured Binding**: Perfectly supports C++17 structured bindings like `for (auto [key, val] : obj.items()) { ... }`, offering extreme convenience identical to iterating a Python dictionary.

## 🏆 PART 4: Target API Benchmarking & "World's Best Usability" Design
We must dominate competing libraries not just in pure performance, but also in "Developer Experience (DX)". We analyze the pros and cons of recent trendsetters `nlohmann/json`, `glaze`, and `rapidjson` to design **qbuem-json's Ultimate API**.

### 4-1. Competitor API Usability Analysis
1. **nlohmann/json ("The King of Intuition")**
   - **Pros**: Operates 100% identically to C++ STL. Insanely intuitive syntax like `json j = "{...}"_json; int age = j["age"];`. Can easily convert structures anywhere (`nlohmann::from_json`).
   - **Cons**: Performance is among the worst in the world due to heavy DOM creation and massive dynamic allocations.
2. **Glaze ("The Pinnacle of Modern Metaprogramming")**
   - **Pros**: Utilizes C++20 `constexpr` metaprogramming to instantly parse JSON strings straight into structures with a single line: `glz::read<MyStruct>(str)`. Both speed and usability are overwhelming.
   - **Cons**: Weak at raw DOM exploration and heavily focused only on full mapping, making it inconvenient when exploring arbitrary JSON without pre-defined structures.
3. **RapidJSON ("The Savior of Performance, but a Nightmare API")**
   - **Pros**: In-situ parsing and incredibly fast, lightweight C-style exploration.
   - **Cons**: Archaic APIs like `document["hello"].GetString()`. If you don't type-check first, it immediately crashes (`Segfault/Throw`). Iterator usage is painfully long.
4. **simdjson ("The Pioneer of Gigabyte/s, but Clunky Error Handling")**
   - **Pros**: Achieves phenomenal speeds via Tape-based DOM parsing utilizing SIMD to the absolute limit.
   - **Cons**: Returns an error code hooked to every single accessor, making chaining difficult. Forces the use of padding blocks (`simdjson::padded_string`) or triggers numerous Exceptions, resulting in a very stiff developer experience.
5. **yyjson ("The Absolute Master of C APIs, devoid of C++ Ideals")**
   - **Pros**: The world's fastest C JSON parser. Pure pointer structure with zero dynamic allocations.
   - **Cons**: Composed of entirely C APIs, completely unable to utilize modern C++ features like structured bindings (`for(auto[k, v])`) or `std::optional` flexibility.

### 4-2. 🔥 The Ultimate Conclusion of Expert Debates: "qbuem-json's Unique Paradigm Design"
While nlohmann's intuition, Glaze's meta-parsing, and simdjson's speed are excellent, they each have critical design limitations (forced Exception throws, complex error codes, lack of flexibility).
After expert deliberation, qbuem-json 1.0 proposes a completely unique paradigm—**"An entirely unique and elegant API found nowhere else in the world"**—combining the latest C++ trends rather than simply mimicking competitors.

Our core paradigm is the **"Zero-Overhead Monadic Proxy"**.

#### 💡 Unique Innovation 1: "Miraculous Fallback utilizing the Pipe (`|`) Operator"
During JSON traversal, if a key is missing or the type is wrong, programs either crash (nlohmann) or force checking complex error codes (simdjson). qbuem-json silently propagates the error state (Monad) upon traversal failure, allowing users to immediately provide a Default value via the C++ Pipe (`|`) operator.
```cpp
// Even if "users" is not an array, its first element is missing, or "age" is missing/not an integer, 
// you securely receive 18 without a single Exception or branch statement (if).
int age = doc["users"][0]["age"] | 18;

// For strings
std::string_view name = doc["users"][0]["name"] | "Guest";
```
This single line of code completely crushes the 10-line `if-else` error handling codes of other libraries.

#### 💡 Unique Innovation 2: "Zero-Allocation Typed Views"
Traditional libraries allocate memory on the heap to create an array when iterating `[1, 2, 3]`. qbuem-json immediately casts C++ types while reading the Tape, offering stream-like iteration capabilities.
```cpp
// Memory allocation (dynamic array creation) size 0 Bytes! Casts to int immediately while reading the tape
for(int id : doc["user_ids"].as_array<int>()) {
    std::cout << id << ", ";
}
```

#### 💡 Unique Innovation 3: "C++20 Compile-Time JSON Pointer"
Introducing string pointer traversal mechanisms validated at compile-time to squeeze runtime traversal overhead to the absolute limit. If the traversal path is fixed, the compiler optimizes it.
```cpp
// Instantly calculates the offset jump to the target path without runtime string parsing
auto timeout = doc.at<"/api/config/timeout">() | 3000;
```

#### 💡 Unique Innovation 4: "Type-Safe Matcher"
When values mutate without a schema (either an int or a string), we break away from clumsy `is_string()`, `is_int()` chaining and offer elegant Rust-style Pattern Matching.
```cpp
doc["variable_field"].match(
    [](int64_t v) { std::cout << "Number: " << v; },
    [](std::string_view s) { std::cout << "String: " << s; },
    []() { std::cout << "Null or Others"; }
);
```

#### 💡 Feature 5: Perfect Dual-Compatibility with Glaze/nlohmann Styles
Even while equipped with all the unique weapons above, we still 100% support 1-Line auto-serialization of C++ `struct`s (`qbuem::read<T>`) and nlohmann-style implicit type conversions like `int a = doc["age"];`.

> **Design Conclusion**: The moment users abandon the archaic and rough APIs of other libraries and taste the Pipe (`|`) operator and Typed Views, they will never be able to return to another JSON library. It is an architecture that will achieve **undisputed Global #1 not just in speed, but in Developer Experience (DX)**.

---

## 📜 PART 3: 100% Compliance with Global JSON Standards (RFCs)
Implementation details to achieve the "integrity" that will strongly appeal to developers upon GitHub release.

1. **RFC 8259 Integrity (Core-Level Defense)**
   - Hardcoded or optional handling of **Max Depth Limits** to defend against Stack Overflow attacks.
   - Integrity support for Unicode Surrogate Pair decoding (e.g., `\uD83D\uDE00`). (Must bolster tests to ensure existing code fully supports it).
2. **Optional RFC Support (Utils-Level Implementation)**
   - **JSON Pointer (RFC 6901)** : Building an accessor in the form of `doc.at("/users/0/name")`.
   - **JSON Patch (RFC 6902)** : Expanding the interface to enable partial DOM modifications without performance degradation.

## 🧩 PART 5: 1.0 Release Verification (Overhead Defense Line)
- **Goal: 0% Overhead in API Layer**:
  - All accessors like `get_int64()` inside `qbuem::Value` must be strictly set to register `inline` to reduce Function Call Stack costs to zero.
  - Test the final bastion of floating-point `get_double()` operations to analytically prove how much delay the Russ Cox algorithm introduces at runtime using `cmp_benchmark.cpp`.
  - Re-verify the structural completeness of the 81 core test suites utilizing `TEST_P` (GTest Equivalent).

## 🌐 PART 6: Foreign Language Bindings (Alien Language Invasion)
In order to dominate global ecosystems rather than just remaining a C++ library, we establish the foundations for integrations (Bindings) with other languages.
- **Pure C-API Export**: Write `extern "C"` interfaces to guarantee FFI compatibility with other languages.
- **Python Module**: Design wrapper modules based on `pybind11` (or `ctypes`) for Python integration, the standard in AI/Data analysis industries. Aimed at replacing the slow builtin Python `json` module.
- **Node.js Addon**: Build C++ native modules based on `N-API` targeting the web server ecosystem.

---

> **Final Conclusion**: Once all structural reorganization in the Blueprint above is complete, qbuem-json will be reborn as the perfect `1.0 Version`, claiming the Global #1 spot not only in Speed (Phase 1-48) but also in **Developer Experience (DX), Stability, and Extensibility**. We are ready to begin.
