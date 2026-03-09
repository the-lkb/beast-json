# Language Bindings

Beast JSON is a C++20 library. This page provides complete, working patterns for calling it from Python, Go, and Rust — including the shim layer each language requires and real usage examples.

---

## Python — nanobind

[nanobind](https://github.com/wjakob/nanobind) is the fastest way to expose a C++ library to Python. The extension below recursively converts a Beast JSON tape into native Python `dict`/`list` objects.

### C++ Extension (`beast_json_py.cpp`)

```cpp
#include <nanobind/nanobind.h>
#include <nanobind/nb_types.h>
#include <beast_json/beast_json.hpp>

namespace nb = nanobind;
using namespace nb::literals;

// Recursively converts a Beast JSON Value to a native Python object.
static nb::object to_py(beast::Value v) {
    if (v.is_object()) {
        auto d = nb::dict();
        for (auto [k, val] : v.items())
            d[nb::str(k.data(), k.size())] = to_py(val);
        return d;
    }
    if (v.is_array()) {
        auto lst = nb::list();
        for (auto elem : v.elements()) lst.append(to_py(elem));
        return lst;
    }
    if (v.is_string()) {
        auto sv = v.as<std::string_view>();
        return nb::str(sv.data(), sv.size());
    }
    if (v.is_int())    return nb::int_(v.as<int64_t>());
    if (v.is_double()) return nb::float_(v.as<double>());
    if (v.is_bool())   return nb::bool_(v.as<bool>());
    return nb::none(); // null
}

NB_MODULE(beast_json_py, m) {
    m.doc() = "Beast JSON Python bindings (nanobind)";

    m.def("loads", [](std::string_view json) -> nb::object {
        beast::Document doc;
        beast::Value root = beast::parse(doc, json);
        return to_py(root);
    }, "json"_a, "Parse a JSON string and return a Python dict/list.");

    // Reuse-mode: parse into a persistent Document to avoid repeated allocation.
    nb::class_<beast::Document>(m, "Document")
        .def(nb::init<>())
        .def("parse", [](beast::Document& doc, std::string_view json) -> nb::object {
            beast::Value root = beast::parse(doc, json);
            return to_py(root);
        }, "json"_a, "Parse JSON, reusing internal tape memory (zero allocation after first call).");
}
```

### Build (`CMakeLists.txt`)

```cmake
cmake_minimum_required(VERSION 3.21)
project(beast_json_py LANGUAGES CXX)

find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(beast_json_py beast_json_py.cpp)
target_include_directories(beast_json_py PRIVATE /path/to/beast-json/include)
target_compile_features(beast_json_py PRIVATE cxx_std_20)
```

### Usage

```python
import beast_json_py as bj

# One-shot parse
data = bj.loads('{"user": "Alice", "score": 42, "tags": ["vip", "beta"]}')
print(data["user"])          # "Alice"
print(data["score"])         # 42
print(data["tags"][0])       # "vip"

# Hot-loop reuse — zero allocation after first parse
doc = bj.Document()
for raw_line in socket_stream:
    row = doc.parse(raw_line)
    process(row["type"], row["payload"])
```

---

## Go — cgo

Go's cgo tool can call C functions directly. Because Beast JSON is C++, we need a thin C-style shim to avoid name mangling. The pattern is: **shim header → shim implementation → Go wrapper → usage**.

### Step 1 — C Shim Header (`beast_shim.h`)

```c
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to a parsed Beast JSON document.
typedef struct BeastDoc BeastDoc;

// Parse JSON. Returns NULL on error. Caller must call beast_free().
BeastDoc* beast_parse   (const char* json, size_t len);
void      beast_free    (BeastDoc* doc);

// Root-level key lookups (object only).
// beast_get_string returns a pointer into the input buffer — valid as long
// as the original json[] passed to beast_parse is alive.
const char* beast_get_string (BeastDoc* doc, const char* key, size_t* out_len);
int         beast_has_key    (BeastDoc* doc, const char* key);
int64_t     beast_get_i64    (BeastDoc* doc, const char* key);
double      beast_get_f64    (BeastDoc* doc, const char* key);
int         beast_get_bool   (BeastDoc* doc, const char* key); // 1=true 0=false

#ifdef __cplusplus
}
#endif
```

### Step 2 — C Shim Implementation (`beast_shim.cpp`)

```cpp
#include "beast_shim.h"
#include <beast_json/beast_json.hpp>
#include <new>

struct BeastDoc {
    std::string        json_copy; // keeps the input buffer alive
    beast::Document    doc;
    beast::Value       root;
};

BeastDoc* beast_parse(const char* json, size_t len) {
    auto* d = new (std::nothrow) BeastDoc();
    if (!d) return nullptr;
    d->json_copy.assign(json, len);            // copy once so Go can free its buffer
    d->root = beast::parse(d->doc, d->json_copy);
    if (!d->root.is_valid()) { delete d; return nullptr; }
    return d;
}

void beast_free(BeastDoc* d) { delete d; }

const char* beast_get_string(BeastDoc* d, const char* key, size_t* out_len) {
    auto v = d->root[key];
    if (!v.is_string()) { *out_len = 0; return nullptr; }
    auto sv = v.as<std::string_view>();
    *out_len = sv.size();
    return sv.data();
}

int beast_has_key(BeastDoc* d, const char* key) {
    return d->root.contains(key) ? 1 : 0;
}

int64_t beast_get_i64(BeastDoc* d, const char* key) {
    return d->root[key].as<int64_t>();
}

double beast_get_f64(BeastDoc* d, const char* key) {
    return d->root[key].as<double>();
}

int beast_get_bool(BeastDoc* d, const char* key) {
    return d->root[key].as<bool>() ? 1 : 0;
}
```

### Step 3 — Go Wrapper (`beastjson/beastjson.go`)

```go
package beastjson

/*
#cgo CXXFLAGS: -std=c++20 -O3 -I/path/to/beast-json/include
#cgo LDFLAGS:  -L/path/to/beast-json/lib -lbeast_shim -lstdc++
#include "beast_shim.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"runtime"
	"unsafe"
)

// Document wraps a parsed Beast JSON document.
// Memory is released automatically when the Go GC collects the object,
// but prefer calling Close() explicitly in performance-sensitive code.
type Document struct {
	h *C.BeastDoc
}

// Parse parses a JSON string. Returns an error if the input is invalid JSON.
func Parse(json string) (*Document, error) {
	cs := C.CString(json)
	defer C.free(unsafe.Pointer(cs))

	h := C.beast_parse(cs, C.size_t(len(json)))
	if h == nil {
		return nil, fmt.Errorf("beast_json: parse failed")
	}

	d := &Document{h: h}
	runtime.SetFinalizer(d, (*Document).Close)
	return d, nil
}

// Close releases the underlying C++ document. Safe to call more than once.
func (d *Document) Close() {
	if d.h != nil {
		C.beast_free(d.h)
		d.h = nil
	}
}

// HasKey reports whether the root object contains a given key.
func (d *Document) HasKey(key string) bool {
	ck := C.CString(key)
	defer C.free(unsafe.Pointer(ck))
	return C.beast_has_key(d.h, ck) == 1
}

// GetString returns the string value for a root-level key.
// The second return value is false if the key is missing or not a string.
func (d *Document) GetString(key string) (string, bool) {
	ck := C.CString(key)
	defer C.free(unsafe.Pointer(ck))

	var outLen C.size_t
	ptr := C.beast_get_string(d.h, ck, &outLen)
	if ptr == nil {
		return "", false
	}
	return C.GoStringN(ptr, C.int(outLen)), true
}

// GetInt64 returns the int64 value for a root-level key.
func (d *Document) GetInt64(key string) int64 {
	ck := C.CString(key)
	defer C.free(unsafe.Pointer(ck))
	return int64(C.beast_get_i64(d.h, ck))
}

// GetFloat64 returns the float64 value for a root-level key.
func (d *Document) GetFloat64(key string) float64 {
	ck := C.CString(key)
	defer C.free(unsafe.Pointer(ck))
	return float64(C.beast_get_f64(d.h, ck))
}

// GetBool returns the bool value for a root-level key.
func (d *Document) GetBool(key string) bool {
	ck := C.CString(key)
	defer C.free(unsafe.Pointer(ck))
	return C.beast_get_bool(d.h, ck) == 1
}
```

### Usage

```go
package main

import (
	"fmt"
	"beastjson"
)

func main() {
	doc, err := beastjson.Parse(`{
		"user":   "Alice",
		"score":  42,
		"ratio":  0.98,
		"active": true
	}`)
	if err != nil {
		panic(err)
	}
	defer doc.Close()

	user, _  := doc.GetString("user")
	score    := doc.GetInt64("score")
	ratio    := doc.GetFloat64("ratio")
	active   := doc.GetBool("active")

	fmt.Printf("user=%s score=%d ratio=%.2f active=%v\n",
		user, score, ratio, active)
	// user=Alice score=42 ratio=0.98 active=true
}
```

> [!TIP]
> The shim copies the JSON input once (`json_copy`) so Go can free its original buffer immediately. All subsequent string reads point into that internal copy — zero extra allocation per key access.

---

## Rust — cxx

Rust's [cxx](https://cxx.rs) crate provides safe, zero-cost C++ interop. You describe the API boundary in a bridge module and cxx generates all the glue code at compile time.

### Step 1 — C++ Shim (`beast_cxx_shim.hpp`)

This thin wrapper exposes a concrete `BjDocument` type that cxx can bind to.

```cpp
#pragma once
#include <beast_json/beast_json.hpp>
#include "rust/cxx.h"
#include <memory>
#include <stdexcept>

struct BjDocument {
    std::string     json_copy; // keeps input buffer alive
    beast::Document doc;
    beast::Value    root;

    rust::Str get_string(rust::Str key) const {
        auto sv = root[to_sv(key)].as<std::string_view>();
        return rust::Str(sv.data(), sv.size());
    }
    int64_t  get_i64 (rust::Str key) const { return root[to_sv(key)].as<int64_t>(); }
    double   get_f64 (rust::Str key) const { return root[to_sv(key)].as<double>(); }
    bool     get_bool(rust::Str key) const { return root[to_sv(key)].as<bool>(); }
    bool     has_key (rust::Str key) const { return root.contains(to_sv(key)); }
    bool     is_valid()              const { return root.is_valid(); }

private:
    static std::string_view to_sv(rust::Str s) { return {s.data(), s.size()}; }
};

inline std::unique_ptr<BjDocument> bj_parse(rust::Str json) {
    auto d = std::make_unique<BjDocument>();
    d->json_copy.assign(json.data(), json.size());
    d->root = beast::parse(d->doc, d->json_copy);
    if (!d->root.is_valid())
        throw std::runtime_error("beast_json: parse error");
    return d;
}
```

### Step 2 — Rust Bridge (`src/lib.rs`)

```rust
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("beast_cxx_shim.hpp");

        type BjDocument;

        /// Parse JSON from a Rust &str. Returns Err on invalid JSON.
        fn bj_parse(json: &str) -> Result<UniquePtr<BjDocument>>;

        fn get_string<'a>(self: &'a BjDocument, key: &str) -> &'a str;
        fn get_i64  (self: &BjDocument, key: &str) -> i64;
        fn get_f64  (self: &BjDocument, key: &str) -> f64;
        fn get_bool (self: &BjDocument, key: &str) -> bool;
        fn has_key  (self: &BjDocument, key: &str) -> bool;
        fn is_valid (self: &BjDocument) -> bool;
    }
}

/// A parsed Beast JSON document.
pub struct Document(cxx::UniquePtr<ffi::BjDocument>);

impl Document {
    pub fn parse(json: &str) -> Result<Self, cxx::Exception> {
        Ok(Document(ffi::bj_parse(json)?))
    }
    pub fn get_str<'a>(&'a self, key: &str) -> &'a str { self.0.get_string(key) }
    pub fn get_i64     (&self, key: &str) -> i64        { self.0.get_i64(key) }
    pub fn get_f64     (&self, key: &str) -> f64        { self.0.get_f64(key) }
    pub fn get_bool    (&self, key: &str) -> bool       { self.0.get_bool(key) }
    pub fn has_key     (&self, key: &str) -> bool       { self.0.has_key(key) }
}
```

### Step 3 — Build Script (`build.rs`)

```rust
fn main() {
    cxx_build::bridge("src/lib.rs")
        .file("beast_cxx_shim.cpp")        // a .cpp that includes beast_cxx_shim.hpp
        .include("/path/to/beast-json/include")
        .std("c++20")
        .flag("-O3")
        .compile("beast_cxx_shim");

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=beast_cxx_shim.hpp");
}
```

### Usage

```rust
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let json = r#"{
        "user":   "Alice",
        "score":  42,
        "ratio":  0.98,
        "active": true
    }"#;

    let doc = Document::parse(json)?;

    println!("user   = {}", doc.get_str("user"));    // Alice
    println!("score  = {}", doc.get_i64("score"));   // 42
    println!("ratio  = {}", doc.get_f64("ratio"));   // 0.98
    println!("active = {}", doc.get_bool("active")); // true

    // Safe key existence check before access
    if doc.has_key("optional_field") {
        println!("opt = {}", doc.get_str("optional_field"));
    }

    Ok(())
}
```

---

## Binding Roadmap

| Language | Status | Package |
|:---|:---|:---|
| Python | Planned | `pip install beast-json` (PyPI wheels) |
| Go | Planned | `go get github.com/beast-json/go-beast` |
| Rust | Planned | `beast-json` on crates.io |
| Node.js | Planned | N-API native addon, `npm install beast-json` |
| Ruby | Exploring | Native C extension via `rice` |
