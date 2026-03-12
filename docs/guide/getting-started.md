# Getting Started

Integrating qbuem-json into your project is designed to be effortless.

## 📦 Installation

qbuem-json is a **single-header library**. Just download `qbuem_json.hpp` and include it.

```bash
# Clone the repo or just grab the header
wget https://raw.githubusercontent.com/qbuem/qbuem-json/main/include/qbuem_json/qbuem_json.hpp
```

### 1. Choice of Engines

qbuem-json offers two main ways to parse JSON data:

#### A. qbuem-json DOM — For Flexible Throughput
Best for general-purpose parsing, large files, or when you need a dynamic DOM.

```cpp
#include <qbuem_json/qbuem_json.hpp>

qbuem::Document doc;
qbuem::Value root = qbuem::parse(doc, json_data);
auto name = root["name"].as<std::string>();
```

#### B. qbuem-json Nexus — For Micro-Latency DTOs
Best for high-frequency objects where every nanosecond counts. It maps JSON **directly** into your struct with **zero intermediate tape**.

```cpp
struct User {
    int id;
    std::string name;
    QBUEM_JSON_FIELDS(User, id, name)
};

// Zero-Tape Fusion
User u = qbuem::fuse<User>(json_data);
```

### 2. High-Performance Serialization

Serialize structs directly using the powerful `QBUEM_JSON_FIELDS` macro.

```cpp
struct Config {
    std::string host;
    int port;
    bool secure;
};

// Registers all fields for automation
QBUEM_JSON_FIELDS(Config, host, port, secure)

Config cfg{"localhost", 8080, true};
std::string out = qbuem::write(cfg);
```

### 3. 🪄 Magic STL Conversions

qbuem-json natively understands standard C++ containers out-of-the-box. No boilerplate, no configuration needed.

```cpp
std::map<std::string, std::vector<int>> data = {
    {"eu-west", {1, 2, 3}},
    {"us-east", {4, 5}}
};

// Serialize directly to string
std::string json = qbuem::write(data);

// Deserialize directly back into your complex STL type
auto parsed = qbuem::read<std::map<std::string, std::vector<int>>>(json);
```

> [!TIP]
> Curious about exactly how C++ types map to JSON schemas? Check out the [Type Mapping Schema](/guide/mapping)!

## 🛠️ Build Configuration

> [!IMPORTANT]
> qbuem-json supports **Linux** and **macOS** only. Windows is not supported.

qbuem-json targets **C++20**. Ensure your compiler flags are set accordingly:

- **GCC**: `-std=c++20 -O3 -march=native`
- **Clang**: `-std=c++20 -O3 -march=native`

For maximum performance on Apple Silicon, use `-mcpu=apple-m1` (or your specific M-series chip).
