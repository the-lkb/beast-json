# Getting Started

Integrating Beast JSON into your project is designed to be effortless.

## 📦 Installation

Beast JSON is a **single-header library**. Just download `beast_json.hpp` and include it.

```bash
# Clone the repo or just grab the header
wget https://raw.githubusercontent.com/qbuem/beast-json/main/include/beast_json/beast_json.hpp
```

## 🚀 Quick Usage

### 1. Parsing JSON

Beast JSON uses a lazy-DOM approach. Parsing is near-instant, and access is type-safe.

```cpp
#include <beast_json/beast_json.hpp>
#include <iostream>

int main() {
    std::string json = R"({"name": "Beast", "speed": 2.7})";

    // A Document owns the tape; a Value is a lightweight handle into it
    beast::Document doc;
    beast::Value root = beast::parse(doc, json);

    // Type-safe access
    std::string name = root["name"].as<std::string>();
    double speed = root["speed"].as<double>();

    std::cout << name << " throughput: " << speed << " GB/s" << std::endl;
}
```

### 2. High-Performance Serialization

Serialize structs directly using the powerful `BEAST_JSON_FIELDS` macro.

```cpp
struct Config {
    std::string host;
    int port;
    bool secure;
};

// Registers all fields for automation
BEAST_JSON_FIELDS(Config, host, port, secure)

Config cfg{"localhost", 8080, true};
std::string out = beast::write(cfg);
```

### 3. 🪄 Magic STL Conversions

Beast JSON natively understands standard C++ containers out-of-the-box. No boilerplate, no configuration needed.

```cpp
std::map<std::string, std::vector<int>> data = {
    {"eu-west", {1, 2, 3}},
    {"us-east", {4, 5}}
};

// Serialize directly to string
std::string json = beast::write(data);

// Deserialize directly back into your complex STL type
auto parsed = beast::read<std::map<std::string, std::vector<int>>>(json);
```

> [!TIP]
> Curious about exactly how C++ types map to JSON schemas? Check out the [Type Mapping Schema](/guide/mapping)!

## 🛠️ Build Configuration

> [!IMPORTANT]
> Beast JSON supports **Linux** and **macOS** only. Windows is not supported.

Beast JSON targets **C++20**. Ensure your compiler flags are set accordingly:

- **GCC**: `-std=c++20 -O3 -march=native`
- **Clang**: `-std=c++20 -O3 -march=native`

For maximum performance on Apple Silicon, use `-mcpu=apple-m1` (or your specific M-series chip).
