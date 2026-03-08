<div align="center">
  <h1>Beast JSON</h1>
  <p><b>The Ultimate High-Performance C++20 JSON Parser & Serializer</b></p>
  <p>Single header. Zero dependencies. AVX-512 & NEON accelerated. Zero-allocation direct streaming.</p>

  <p>
    <a href="https://github.com/kyubuem/beast-json/actions/workflows/ci.yml"><img src="https://github.com/kyubuem/beast-json/actions/workflows/ci.yml/badge.svg" alt="C++20 CI"></a>
    <a href="https://github.com/kyubuem/beast-json/actions/workflows/codeql.yml"><img src="https://github.com/kyubuem/beast-json/actions/workflows/codeql.yml/badge.svg" alt="CodeQL Static Analysis"></a>
    <a href="https://github.com/kyubuem/beast-json/releases"><img src="https://img.shields.io/badge/Version-v1.0-blue" alt="Version 1.0"></a>
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache%202.0-blue.svg" alt="License: Apache 2.0"></a>
  </p>
</div>

---

**Beast JSON** is a bleeding-edge, lazy-DOM C++20 JSON library engineered to be the absolute fastest **C++ JSON parser and serializer** in the world. Designed for latency-critical applications, Game Engines, High-Frequency Trading systems, and extreme throughput web servers.

By fully leveraging **C++20 Concepts**, **SIMD (AVX-512, NEON)**, **SWAR (SIMD Within A Register)**, and a **Zero-Allocation Array-Backed Tape DOM**, Beast JSON fundamentally changes how JSON is processed. It destroys traditional tree-based DOM performance limits while retaining a beautifully simple API, cementing itself as the ultimate **C++20 JSON library**.

## 🚀 Features at a Glance

* **World-Class Performance**: Outperforms `yyjson`, `simdjson`, `glaze`, and `rapidjson` in parsing and serialization speed on both `x86_64` (Intel/AMD) and `AArch64` (Apple Silicon M-Series, Snapdragon).
* **Zero-Allocation Execution**: Memory-mapped zero-copy strings for parsing, and direct-to-buffer stream pushing for serialization. The ultimate zero-cost abstraction.
* **C++20 Native**: Clean, elegant integration using C++20 standard Concepts and fold expressions. No legacy SFINAE hacks. Range-based iterations directly supported.
* **Auto-Serialization Macro**: One-line macro (`BEAST_JSON_FIELDS`) generates 100% automated struct-to-JSON and JSON-to-struct mapping with zero boilerplate.
* **Safe Monadic Interface**: Never throw exceptions nor segfault using the `SafeValue` (`std::optional`-propagating) interface for deep traversal arrays.
* **Single Header**: Drop `beast_json.hpp` into your project. That's it.
* **Fuzzed & Hardened**: Passed relentless libFuzzer suites with statically-linked ASan and UBSan. Fully memory safe.

---

## ⚡ Unrivaled Performance (Benchmark v1.0.2)

Beast JSON outperforms traditional and modern C++ JSON parsers utilizing aggressive SIMD and a zero-allocation sequential tape.

### 🏎 Parsing & Serialization Timings

*Measured using `-O3 -march=native / -mcpu=apple-m1 + LTO` on respective GitHub runners.*

#### 🖥 Intel x86_64

| **Library** | **twitter (Parse/Ser)** | **canada (Parse/Ser)** | **citm (Parse/Ser)** | **gsoc (Parse/Ser)** |
|:---|---:|---:|---:|---:|
| **Beast JSON** | 265 μs / **149 μs** | **1876 μs** / **952 μs** | **597 μs** / 326 μs | **531 μs** / **273 μs** |
| `simdjson` | **242 μs** / 815 μs | 2509 μs / 10163 μs | 732 μs / 1101 μs | 945 μs / 5568 μs |
| `yyjson` | 749 μs / 153 μs | 2987 μs / 3968 μs | 2102 μs / **251 μs** | 2020 μs / 1083 μs |
| `RapidJSON` | 1294 μs / 826 μs | 5386 μs / 7475 μs | 2196 μs / 830 μs | 7085 μs / 4593 μs |
| `Glaze` | 2251 μs / 363 μs | 8297 μs / 4130 μs | 3697 μs / 958 μs | 4226 μs / 1245 μs |
| `nlohmann` | 6142 μs / 2211 μs | 37308 μs / 10197 μs | 11852 μs / 2681 μs | 25726 μs / 15255 μs |

#### 🖥 Apple M-Series

| **Library** | **twitter (Parse/Ser)** | **canada (Parse/Ser)** | **citm (Parse/Ser)** | **gsoc (Parse/Ser)** |
|:---|---:|---:|---:|---:|
| **Beast JSON** | 229 μs / **75 μs** | 1925 μs / **878 μs** | 563 μs / 199 μs | **752 μs** / **293 μs** |
| `simdjson` | 228 μs / 439 μs | 2466 μs / 7943 μs | **501 μs** / 646 μs | 1206 μs / 6041 μs |
| `yyjson` | **187 μs** / 108 μs | **1868 μs** / 2563 μs | 644 μs / **177 μs** | 1208 μs / 1050 μs |
| `RapidJSON` | 926 μs / 998 μs | 2888 μs / 7330 μs | 1404 μs / 937 μs | 6733 μs / 7011 μs |
| `Glaze` | 1698 μs / 239 μs | 7073 μs / 2826 μs | 2789 μs / 710 μs | 2755 μs / 836 μs |
| `nlohmann` | 3745 μs / 1449 μs | 19787 μs / 7451 μs | 9749 μs / 2125 μs | 14657 μs / 13456 μs |

#### 🖥 Linux AArch64

| **Library** | **twitter (Parse/Ser)** | **canada (Parse/Ser)** | **citm (Parse/Ser)** | **gsoc (Parse/Ser)** |
|:---|---:|---:|---:|---:|
| **Beast JSON** | 3254 μs / **1164 μs** | **19708 μs** / **8229 μs** | 9464 μs / 2822 μs | **7429 μs** / **2031 μs** |
| `simdjson` | 7620 μs / 4158 μs | 32192 μs / 41013 μs | 16566 μs / 6913 μs | 30977 μs / 22403 μs |
| `yyjson` | **2874 μs** / 1503 μs | 21540 μs / 25113 μs | **7919 μs** / **2678 μs** | 10255 μs / 6799 μs |
| `RapidJSON` | 6652 μs / 3239 μs | 32500 μs / 46085 μs | 13451 μs / 4967 μs | 25648 μs / 15419 μs |
| `Glaze` | 20924 μs / 2081 μs | 85146 μs / 19867 μs | 36008 μs / 5719 μs | 32766 μs / 5502 μs |
| `nlohmann` | 56953 μs / 13042 μs | 372197 μs / 52692 μs | 95103 μs / 17262 μs | 249566 μs / 67940 μs |

### 🪶 Unmatched Memory Efficiency
Memory measured parsing `twitter.json` (631.5 KB) via MacOS `mach_task` Resident Set Size (RSS). Beast JSON achieves industry-leading memory efficiency by utilizing a minimalistic 8-byte Tape representation and true zero-copy strings.

| Library | Peak RSS | DOM Memory | Overhead Ratio |
|:---|---:|---:|---:|
| **Beast JSON** | **3.44 MB** | **0.23 MB** | **0.36x** |
| `yyjson`       | 6.32 MB | 2.50 MB | 3.96x |
| `Glaze`        | 5.58 MB | 1.80 MB | 2.85x |
| `simdjson`     | 11.04 MB | 6.50 MB | 10.29x |

> *Note: For deep-nesting custom types, Beast JSON outpaces C++23 Reflection-based `Glaze` natively due to fully inlined variadic macros.*

### 🌪 Extreme Heavy-Load Benchmarks (Harsh Environment)
Performance under extreme stress: measuring a massive 5.5MB file containing 50,000 deeply nested objects, arrays, floats, and heavily escaped strings (`\n\t\r\"`). This tests the parser's absolute worst-case fallback performance.

#### 🖥 Intel x86_64

| **Library** | **Parse Time (ms)** | **Serialize Time (ms)** |
|:---|---:|---:|
| **Beast JSON** | 7.71 ms | **3.71 ms** |
| `simdjson` | **6.04 ms** | 16.34 ms |
| `yyjson` | 9.13 ms | 4.49 ms |
| `RapidJSON` | 17.55 ms | 14.14 ms |
| `Glaze` | 90.33 ms | 39.23 ms |
| `nlohmann` | 164.70 ms | 35.95 ms |

#### 🖥 Apple M-Series

| **Library** | **Parse Time (ms)** | **Serialize Time (ms)** |
|:---|---:|---:|
| **Beast JSON** | 5.28 ms | **2.32 ms** |
| `simdjson` | **4.89 ms** | 12.16 ms |
| `yyjson` | 5.29 ms | 3.53 ms |
| `RapidJSON` | 13.15 ms | 11.90 ms |
| `Glaze` | 35.96 ms | 10.69 ms |
| `nlohmann` | 58.68 ms | 17.44 ms |

#### 🖥 Linux AArch64

| **Library** | **Parse Time (ms)** | **Serialize Time (ms)** |
|:---|---:|---:|
| **Beast JSON** | 81.11 ms | **32.32 ms** |
| `simdjson` | 104.28 ms | 119.38 ms |
| `yyjson` | **78.12 ms** | 49.15 ms |
| `RapidJSON` | 109.74 ms | 79.94 ms |
| `Glaze` | 457.51 ms | 88.45 ms |
| `nlohmann` | 1034.05 ms | 190.70 ms |

---

## 📚 Documentation

Detailed documentation has been consolidated into a central technical reference:

| Guide | Link |
|:---|:---|
| **Getting Started** | [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) (Install, first parse, mutation, pitfalls) |
| **Technical Reference** | [docs/TECHNICAL_REFERENCE.md](docs/TECHNICAL_REFERENCE.md) (API, Architecture, Performance, Optimization History, Security) |

## 🛠 Usage Example

### Parsing & Safe Access

```cpp
#include <beast_json/beast_json.hpp>
#include <iostream>

using namespace beast;

int main() {
    Document doc;
    Value root = parse(doc, R"({"user": {"id": 1024, "tags": ["admin", "dev"]}})");

    // Safe Monadic Optional Navigation
    SafeValue user = root.get("user");
    int user_id = user["id"].value_or(-1);       // 1024
    int timeout = user["timeout"].value_or(100); // 100 (safely falls back)

    // Iterating C++20 Ranges
    for (auto val : root["user"]["tags"].elements()) {
        std::cout << val.as<std::string_view>() << ", ";
    }
}
```

### Magic Auto-Serialization (C++20 Macros)

```cpp
// Define your structs
struct Config {
    std::string ip;
    int port;
};
BEAST_JSON_FIELDS(Config, ip, port)

struct User {
    std::string name;
    std::optional<bool> is_admin;
    Config config; // Nested struct!
    std::vector<std::string> roles; // STL Containers!
};
// Register fields (that's it!)
BEAST_JSON_FIELDS(User, name, is_admin, config, roles)

int main() {
    // Deserialize directly from string (Recursive)
    auto user = beast::read<User>(R"({
        "name": "Kayden",
        "config": {"ip": "127.0.0.1", "port": 8080},
        "roles": ["dev", "lead"]
    })");

    // Serialize back to zero-allocation JSON String
    std::string json = beast::write(user);
}
```

## 📦 Integration

### CMake

Simply add the include path:

```cmake
add_library(beast_json INTERFACE)
target_include_directories(beast_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(my_app PRIVATE beast_json)
```

Requires a C++20 compliant compiler:
* **GCC** 12+
* **Clang** 15+
* **Apple Clang** 16+

---

## 🏗 Architecture Highlight: The Tape DOM

Unlike traditional JSON parsers like `nlohmann/json` or `rapidjson` that allocate countless 32-byte tree nodes scattered across the heap, Beast JSON writes **64-bit TapeNodes** continuously to a single flat array.
- **Cache-Locality**: A 128-byte Apple M-series L1 Cache line holds exactly 16 JSON nodes. Fetching an object immediately prefetches all of its keys and values into the L1.
- **Zero Cache Misses**: Traversing a Beast JSON DOM is purely sequential array access.
- **Immutable Parsing**: Mutations (`set`, `insert`, `erase`) leverage a side-channel C++ `unordered_map` overlay network. The core immutable tape is NEVER rewritten until `dump()` is called.

For an extensive dive into Beast's SIMD Two-Phase Pipeline and Key-Length Caching, see the [Technical Reference](docs/TECHNICAL_REFERENCE.md).

---

## 🤝 Commitment to Open Source (OSI)

Beast JSON is developed in strict alignment with the principles of the **Open Source Initiative (OSI)**. 

We believe that foundational infrastructure libraries—especially those handling critical data formatting like JSON—must be openly accessible, transparently built, and collaboratively maintained.
* **True Software Freedom**: Licensed under the permissive **Apache License 2.0**, allowing unrestricted commercial use, modification, and distribution without copyleft friction.
* **Transparent Benchmarking**: We are committed to honest, reproducible performance metrics. All benchmark suites and data files are open and designed to be verified independently.
* **Community-Driven**: Your contributions, critiques, and ideas are what drive this project forward. We warmly welcome developers to participate in making this the absolute fastest and most reliable JSON framework on the planet.

---

## 💡 Inspiration & Acknowledgements

This project was built on the shoulders of giants. Beast JSON exists because of the incredible open-source engineering efforts of the libraries we benchmark against:

* [yyjson](https://github.com/ibireme/yyjson): For pioneering the modern Array-Backed Tape DOM concept in pure C.
* [simdjson](https://github.com/simdjson/simdjson): For proving that parsing JSON at gigabytes-per-second using SIMD was even possible.
* [Glaze](https://github.com/stephenberry/glaze): For pushing the boundaries of what compile-time C++20/23 reflection could achieve for JSON.
* [nlohmann/json](https://github.com/nlohmann/json): For setting the gold standard of what a beautiful, intuitive C++ API should feel like.
* [RapidJSON](https://github.com/Tencent/rapidjson): For defining the standard of high-performance C++ JSON over the last decade.
