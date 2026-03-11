# Type Mapping & Macros

Beast JSON uses a **Zero-Boilerplate Design**. It automatically deduces exactly how your C++ types should be represented in JSON — no configuration, no registration, no code generation step.

## 🪄 STL Type Mapping Schema

You don't need to write any conversion code for standard library containers. Just pass them to `beast::write()` or `beast::read<T>()`.

| C++ Type | JSON Schema | Example Input | JSON Output |
| :--- | :--- | :--- | :--- |
| **`std::vector<T>`**<br/>`std::list`, `std::deque` | Array `[ ... ]` | `{1, 2, 3}` | `[1,2,3]` |
| **`std::array<T, N>`** | Fixed Array `[ ... ]` | `{1.0, 2.0}` | `[1.0,2.0]` |
| **`std::set<T>`**<br/>`std::unordered_set` | Sorted Array | `{3, 1, 2}` | `[1,2,3]` |
| **`std::map<string, T>`**<br/>`std::unordered_map` | Object `{ ... }` | `{{"a", 1}}` | `{"a":1}` |
| **`std::optional<T>`** | Value or `null` | `std::nullopt` | `null` |
| **`std::tuple<T...>`** | Heterogeneous Array | `{1, "A", true}` | `[1,"A",true]` |
| **`std::pair<T1, T2>`** | 2-element Array | `{"key", 42}` | `["key",42]` |
| **`std::variant<T...>`** | Dynamic Value Match | `(holds int) 123` | `123` |

> [!TIP]
> **This mapping is fully recursive!** A `std::map<std::string, std::vector<std::optional<int>>>` works perfectly out of the box.
>
> ```cpp
> auto data = beast::read<std::map<std::string, std::vector<int>>>(
>     R"({"scores": [95, 87, 100], "ids": [1, 2]})"
> );
> // data["scores"][0] == 95
> ```

---

## 🛠️ Choice of Engines: DOM vs. Nexus

When mapping JSON to custom structs, Beast JSON allows you to choose your performance profile based on your data scale.

### 1. The Standard Path: `beast::read<T>` (Tape-DOM)
Best for **general-purpose bulk data**, large arrays, or when schemas are slightly fluid. It uses the Stage 1 SIMD scanner to build a contiguous Tape, which is then mapped to your struct.

```cpp
// 1. Build Tape (SIMD)
// 2. Map to Struct
User u = beast::read<User>(json_str);
```

### 2. The Nexus Path: `beast::fuse<T>` (Zero-Tape)
Best for **latency-critical micro-DTOs**. It bypasses the Tape entirely, using **Nexus Fusion** technology to stream JSON directly into your struct members using $O(1)$ perfect-hash dispatch.

```cpp
// 1. Direct Stream-to-Struct mapping
// 0.0 Tape Allocation
User u = beast::fuse<User>(json_str);
```

---

## 🏗️ Direct Struct Mapping with `BEAST_JSON_FIELDS`

For your own types, the `BEAST_JSON_FIELDS` macro auto-generates optimized metadata used by both engines. Place it **outside** the struct definition.

```cpp
struct Address {
    std::string street;
    std::string city;
    std::string country;
};
BEAST_JSON_FIELDS(Address, street, city, country)

struct User {
    uint64_t    id;
    std::string username;
    Address     address;                              // nested struct — works automatically!
    std::vector<std::string> tags;                   // STL container — works automatically!
    std::optional<double>    score;                  // optional — maps to null when empty
    bool        active = true;                       // default values are preserved
};
BEAST_JSON_FIELDS(User, id, username, address, tags, score, active)
```

> [!IMPORTANT]
> To use `beast::fuse<T>`, the struct **must** be registered with `BEAST_JSON_FIELDS` and use C++20 standard layout types where possible for maximum speed.

### Performance Tip: Perfect Hash Dispatch
Unlike other libraries that use runtime string comparisons, `BEAST_JSON_FIELDS` computes **FNV-1a hashes at compile-time**. Whether your struct has 3 fields or 30, field lookup is always $O(1)$.

> [!NOTE]
> `BEAST_JSON_FIELDS` now supports up to **32 fields** per struct. For larger structures, see the manual ADL hooks section below.

---

## ✏️ Mutating Parsed JSON

Beast JSON supports **non-destructive mutations** — the original tape is immutable, and changes are stored in a fast overlay map.

### Scalar Mutation

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"user": {"id": 1, "name": "Alice", "score": 87.5}})");

// Override scalar values
root["user"]["id"]    = 99;
root["user"]["name"]  = "Bob";
root["user"]["score"] = 100.0;
root["user"]["active"] = true;    // can add new scalar fields
root["user"]["extra"] = nullptr;  // null

// Immediately reflected in dump()
std::cout << root["user"].dump() << "\n";
// {"id":99,"name":"Bob","score":100.0,"active":true,"extra":null}

// Restore original parsed value
root["user"]["id"].unset();
std::cout << root["user"]["id"].as<int>() << "\n"; // 1 (original restored)
// ⚠️  unset() reverts to the *original parsed value*, NOT null.
//     After unset(), type_name() and as<T>() reflect the original tape entry.
//     unset() only removes the scalar mutation overlay; keys added via insert()
//     or elements added via push_back() are NOT affected by unset().
```

### Structural Mutations (Add / Remove / Append)

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"tags": ["cpp", "json"], "config": {}})");

// Object: add new key
root.insert("version", 2);
root.insert("label", std::string_view{"preview"});

// Object: add a nested JSON subtree
root.insert_json("meta", R"({"build": "release", "arch": "x86_64"})");

// Object: remove a key
root.erase("deprecated_field");

// Array: append elements
root["tags"].push_back(std::string_view{"simd"});              // "simd"
root["tags"].push_back_json(R"({"nested": "object"})");        // object element

// Array: remove by index (accepts size_t or unsigned int)
root["tags"].erase(0u);  // removes "cpp"

// All changes reflected immediately
std::cout << root.dump() << "\n";
// size() reflects both tape elements AND push_back() additions
std::cout << root["tags"].size() << "\n"; // 3 (original 2 + 1 push_back)
// items() includes both tape keys AND insert() additions
for (auto [k, v] : root.items()) { /* iterates original + inserted keys */ }
```

### Merging JSON (RFC 7396 Merge Patch)

```cpp
beast::Document doc;
auto root = beast::parse(doc, R"({"a": 1, "b": 2, "c": 3})");

// merge_patch: adds/updates fields from patch, removes fields set to null
root.merge_patch(R"({"b": 99, "c": null, "d": "new"})");

std::cout << root.dump() << "\n";
// {"a":1,"b":99,"d":"new"}  ("c" was removed because it was null in the patch)
```

---

## 🔌 Advanced: ADL & Custom Hooks (Nexus Engine)

If you need to support third-party types that cannot be modified with macros, or if your struct exceeds 32 fields, you can manually implement the **ADL Hooks**.

The Nexus Engine (`beast::fuse`) specifically looks for `nexus_pulse` via Argument-Dependent Lookup.

```cpp
// Manual Nexus Hook for a 3rd party type
namespace third_party {
    struct Custom { int x; };

    inline void nexus_pulse(std::string_view key, const char*& p, const char* end, Custom& obj) {
        // High-performance dispatch using FNV-1a hashes
        switch (beast::json::detail::fnv1a_hash(key)) {
            case beast::json::detail::fnv1a_hash_ce("x"):
                beast::json::detail::from_json_direct(p, end, obj.x);
                break;
            default:
                beast::json::detail::skip_direct(p, end);
                break;
        }
    }
}
```

By defining `nexus_pulse` in the same namespace as your type, `beast::fuse<T>` will automatically use it for direct, zero-tape mapping. 

---

## 🔧 Third-Party Types via ADL (DOM Engine)

If you **cannot** modify a struct (e.g., from a library like `glm`), define Argument-Dependent Lookup (ADL) functions in the **same namespace** as the type:

```cpp
#include <glm/vec3.hpp>

namespace glm {

    // Teach Beast JSON how to parse glm::vec3 from [x, y, z]
    inline void from_beast_json(const beast::Value& v, vec3& out) {
        out.x = v[0u].as<float>();
        out.y = v[1u].as<float>();
        out.z = v[2u].as<float>();
    }

    // Teach Beast JSON how to serialize glm::vec3 to [x, y, z]
    inline void to_beast_json(beast::Value& root, const vec3& in) {
        root = beast::Value::array();
        root.push_back(beast::Value(in.x));
        root.push_back(beast::Value(in.y));
        root.push_back(beast::Value(in.z));
    }
}

// Now glm::vec3 works with Beast JSON natively!
glm::vec3 pos = beast::read<glm::vec3>("[1.0, 2.0, 3.5]");
std::string json = beast::write(pos);  // "[1.0,2.0,3.5]"
```

This also works for nested structs containing third-party types:

```cpp
struct Transform {
    glm::vec3 position;
    glm::vec3 rotation;
};
BEAST_JSON_FIELDS(Transform, position, rotation)  // glm::vec3 is automatically handled!
```

---

## 🎮 Complete Real-World Example

Here's how Beast JSON handles a full game player profile:

```cpp
struct Stats {
    int    kills    = 0;
    int    deaths   = 0;
    double kd_ratio = 0.0;
};
BEAST_JSON_FIELDS(Stats, kills, deaths, kd_ratio)

struct Equipment {
    std::string weapon;
    std::string armor;
    std::vector<std::string> consumables;
};
BEAST_JSON_FIELDS(Equipment, weapon, armor, consumables)

struct Player {
    uint64_t    id;
    std::string name;
    int         level   = 1;
    Stats       stats;
    Equipment   equip;
    std::optional<std::string> guild;
    std::vector<std::string>   achievements;
};
BEAST_JSON_FIELDS(Player, id, name, level, stats, equip, guild, achievements)

int main() {
    // --- Deserialize from API response ---
    std::string api_json = R"({
        "id": 123456, "name": "DragonSlayer", "level": 87,
        "stats": {"kills": 500, "deaths": 42, "kd_ratio": 11.9},
        "equip": {"weapon": "Excalibur", "armor": "Dragon Mail", "consumables": ["Potion", "Elixir"]},
        "guild": "Knights of qbuem",
        "achievements": ["First Blood", "Legend"]
    })";

    Player p = beast::read<Player>(api_json);
    std::cout << p.name << " (Level " << p.level << ")\n";
    std::cout << "K/D: " << p.stats.kd_ratio << "\n";
    std::cout << "Guild: " << p.guild.value_or("No Guild") << "\n";

    // --- Modify and Serialize back ---
    p.level = 88;
    p.achievements.push_back("Legendary Warrior");

    std::string updated_json = beast::write(p, 2); // pretty-print
    std::cout << updated_json << "\n";
}
```
