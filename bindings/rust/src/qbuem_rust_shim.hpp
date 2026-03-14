#pragma once
#include <qbuem_json/qbuem_json.hpp>
#include <memory>
#include <string>
#include <string_view>
#include "rust/cxx.h"

namespace qbuem::rust {
using Value = qbuem::Value;

class PyDocument {
public:
    qbuem::Document doc;
    qbuem::Value    root;
    std::string     source;

    void parse(::rust::Str json) {
        source = std::string(json);
        root = qbuem::parse(doc, source);
        if (!root.is_valid()) {
            throw std::runtime_error("qbuem-json: parse error");
        }
    }

    qbuem::Value get_root() const { return root; }
};

class ObjectIterator {
public:
    using NativeIter = decltype(std::declval<qbuem::Value>().items().begin());
    NativeIter current;
    NativeIter end;
    bool started = false;

    ObjectIterator(NativeIter begin, NativeIter end) : current(begin), end(end) {}
};

std::unique_ptr<PyDocument> create_doc();
void parse(PyDocument &doc, ::rust::Str json);
std::unique_ptr<qbuem::Value> root(const PyDocument &doc);

// Value wrappers for cxx
bool is_valid(const qbuem::Value &v);
bool is_null(const qbuem::Value &v);
bool is_bool(const qbuem::Value &v);
bool is_int(const qbuem::Value &v);
bool is_double(const qbuem::Value &v);
bool is_string(const qbuem::Value &v);
bool is_array(const qbuem::Value &v);
bool is_object(const qbuem::Value &v);

size_t size(const qbuem::Value &v);
::rust::Str type_name(const qbuem::Value &v);

std::unique_ptr<qbuem::Value> get_key(const qbuem::Value &v, ::rust::Str key);
std::unique_ptr<qbuem::Value> get_idx(const qbuem::Value &v, size_t idx);
std::unique_ptr<qbuem::Value> at_path(const qbuem::Value &v, ::rust::Str path);

// Iteration
std::unique_ptr<ObjectIterator> create_iter(const qbuem::Value &v);
bool iter_next(ObjectIterator &it);
::rust::Str iter_key(const ObjectIterator &it);
std::unique_ptr<qbuem::Value> iter_value(const ObjectIterator &it);

bool as_bool(const qbuem::Value &v);
int64_t as_i64(const qbuem::Value &v);
double as_f64(const qbuem::Value &v);
::rust::Str as_str(const qbuem::Value &v);

::rust::String dump(const qbuem::Value &v, int32_t indent);

// Mutation
void set_null(qbuem::Value &v);
void set_bool(qbuem::Value &v, bool b);
void set_int(qbuem::Value &v, int64_t i);
void set_double(qbuem::Value &v, double d);
void set_string(qbuem::Value &v, ::rust::Str s);

void insert_raw(qbuem::Value &v, ::rust::Str key, ::rust::Str raw_json);
void erase_key(qbuem::Value &v, ::rust::Str key);
void erase_idx(qbuem::Value &v, size_t idx);

} // namespace qbuem::rust
