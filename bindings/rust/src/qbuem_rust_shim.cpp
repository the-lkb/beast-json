#include "qbuem_rust_shim.hpp"
#include <stdexcept>

namespace qbuem::rust {

std::unique_ptr<PyDocument> create_doc() {
    return std::make_unique<PyDocument>();
}

void parse(PyDocument &doc, ::rust::Str json) {
    doc.parse(json);
}

std::unique_ptr<Value> root(const PyDocument &doc) {
    return std::make_unique<Value>(doc.get_root());
}

bool is_valid(const Value &v) { return v.is_valid(); }
bool is_null(const Value &v) { return v.is_null(); }
bool is_bool(const Value &v) { return v.is_bool(); }
bool is_int(const Value &v) { return v.is_int(); }
bool is_double(const Value &v) { return v.is_double(); }
bool is_string(const Value &v) { return v.is_string(); }
bool is_array(const Value &v) { return v.is_array(); }
bool is_object(const Value &v) { return v.is_object(); }

size_t size(const Value &v) { return v.size(); }

::rust::Str type_name(const Value &v) {
    auto sv = v.type_name();
    return ::rust::Str(sv.data(), sv.size());
}

std::unique_ptr<Value> get_key(const Value &v, ::rust::Str key) {
    return std::make_unique<Value>(v[std::string(key)]);
}

std::unique_ptr<Value> get_idx(const Value &v, size_t idx) {
    return std::make_unique<Value>(v[idx]);
}

std::unique_ptr<Value> at_path(const Value &v, ::rust::Str path) {
    return std::make_unique<Value>(v.at(std::string(path)));
}

std::unique_ptr<ObjectIterator> create_iter(const Value &v) {
    if (!v.is_object()) return nullptr;
    auto items = v.items();
    return std::make_unique<ObjectIterator>(items.begin(), items.end());
}

bool iter_next(ObjectIterator &it) {
    if (!it.started) {
        it.started = true;
    } else if (it.current != it.end) {
        ++it.current;
    }
    return it.current != it.end;
}

::rust::Str iter_key(const ObjectIterator &it) {
    auto sv = std::get<0>(*it.current);
    return ::rust::Str(sv.data(), sv.size());
}

std::unique_ptr<Value> iter_value(const ObjectIterator &it) {
    return std::make_unique<Value>(std::get<1>(*it.current));
}

bool as_bool(const Value &v) { return v.as<bool>(); }
int64_t as_i64(const Value &v) { return v.as<int64_t>(); }
double as_f64(const Value &v) { return v.as<double>(); }

::rust::Str as_str(const Value &v) {
    auto sv = v.as<std::string_view>();
    return ::rust::Str(sv.data(), sv.size());
}

::rust::String dump(const Value &v, int32_t indent) {
    return ::rust::String(v.dump(indent));
}

void set_null(Value &v) { v.set(nullptr); }
void set_bool(Value &v, bool b) { v.set(b); }
void set_int(Value &v, int64_t i) { v.set(i); }
void set_double(Value &v, double d) { v.set(d); }
void set_string(Value &v, ::rust::Str s) { v.set(std::string_view(s.data(), s.size())); }

void insert_raw(Value &v, ::rust::Str key, ::rust::Str raw_json) {
    v.insert(std::string(key), std::string(raw_json));
}
void erase_key(Value &v, ::rust::Str key) { v.erase(std::string(key)); }
void erase_idx(Value &v, size_t idx) { v.erase(idx); }

} // namespace qbuem::rust
