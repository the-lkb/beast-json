/**
 * qbuem_json_c.cpp — qbuem-json C API implementation
 */

#include "qbuem_json_c.h"
#include <qbuem_json/qbuem_json.hpp>

#include <cstring>
#include <string>
#include <vector>

// ── Internal Helpers ─────────────────────────────────────────────────────────

static qbuem::Value to_cpp(QbuemJSONValue v) {
  return qbuem::Value(static_cast<qbuem::json::DocumentState*>(v._internal_doc), v._internal_idx);
}

static QbuemJSONValue from_cpp(qbuem::Value v) {
  return {v.state(), v.index()};
}

static QbuemJSONValue invalid_val() {
  return {nullptr, 0};
}

struct QbuemJSONDocument_ {
  qbuem::Document doc;
  qbuem::Value    root;
  std::string     source;
  std::string     last_error;
  std::string     dump_buf;
};

// ── Document lifecycle ────────────────────────────────────────────────────────

QbuemJSONDocument* qbuem_json_doc_create() {
  try { return new QbuemJSONDocument_(); }
  catch (...) { return nullptr; }
}

void qbuem_json_doc_destroy(QbuemJSONDocument* doc) {
  delete doc;
}

// ── Parsing ───────────────────────────────────────────────────────────────────

QbuemJSONValue qbuem_json_parse(QbuemJSONDocument* doc,
                       const char* json_data, size_t json_len) {
  if (!doc || !json_data) return invalid_val();
  try {
    doc->source.assign(json_data, json_len);
    doc->last_error.clear();
    qbuem::Value root = qbuem::parse(doc->doc, doc->source);
    doc->root = root;
    return from_cpp(root);
  } catch (const std::exception& e) {
    doc->last_error = e.what();
    return invalid_val();
  }
}

QbuemJSONValue qbuem_json_parse_strict(QbuemJSONDocument* doc,
                              const char* json_data, size_t json_len) {
  if (!doc || !json_data) return invalid_val();
  try {
    doc->last_error.clear();
    qbuem::Value root = qbuem::parse_strict(doc->doc,
                                             std::string_view(json_data, json_len));
    return from_cpp(root);
  } catch (const std::exception& e) {
    doc->last_error = e.what();
    return invalid_val();
  }
}

const char* qbuem_json_last_error(const QbuemJSONDocument* doc) {
  if (!doc) return "null document";
  return doc->last_error.c_str();
}

QbuemJSONValue qbuem_json_doc_get_root(const QbuemJSONDocument* doc) {
  if (!doc) return invalid_val();
  return from_cpp(doc->root);
}

// ── Value introspection ───────────────────────────────────────────────────────

QbuemJSONType qbuem_json_type(QbuemJSONValue val) {
  auto v = to_cpp(val);
  if (!v.is_valid()) return QBUEM_JSON_TYPE_INVALID;
  if (v.is_null())   return QBUEM_JSON_TYPE_NULL;
  if (v.is_bool())   return QBUEM_JSON_TYPE_BOOL;
  if (v.is_int())    return QBUEM_JSON_TYPE_INT;
  if (v.is_double()) return QBUEM_JSON_TYPE_DOUBLE;
  if (v.is_string()) return QBUEM_JSON_TYPE_STRING;
  if (v.is_array())  return QBUEM_JSON_TYPE_ARRAY;
  if (v.is_object()) return QBUEM_JSON_TYPE_OBJECT;
  return QBUEM_JSON_TYPE_INVALID;
}

const char* qbuem_json_type_name(QbuemJSONValue val) {
  auto v = to_cpp(val);
  if (!v.is_valid()) return "invalid";
  return v.type_name().data();
}

int qbuem_json_is_valid(QbuemJSONValue val) {
  return to_cpp(val).is_valid() ? 1 : 0;
}

// ── Scalar extraction ─────────────────────────────────────────────────────────

int qbuem_json_as_bool(QbuemJSONValue val) {
  auto v = to_cpp(val);
  try { return v.as<bool>() ? 1 : 0; } catch (...) { return 0; }
}

int64_t qbuem_json_as_int(QbuemJSONValue val) {
  auto v = to_cpp(val);
  try { return v.as<int64_t>(); } catch (...) { return 0; }
}

double qbuem_json_as_double(QbuemJSONValue val) {
  auto v = to_cpp(val);
  try {
    if (v.is_int()) return static_cast<double>(v.as<int64_t>());
    return v.as<double>();
  } catch (...) { return 0.0; }
}

const char* qbuem_json_as_string(QbuemJSONValue val, size_t* out_len) {
  auto v = to_cpp(val);
  try {
    auto sv = v.as<std::string_view>();
    if (out_len) *out_len = sv.size();
    return sv.data();
  } catch (...) { return nullptr; }
}

// ── Container access ──────────────────────────────────────────────────────────

size_t qbuem_json_size(QbuemJSONValue val) {
  auto v = to_cpp(val);
  try { return v.size(); } catch (...) { return 0; }
}

int qbuem_json_empty(QbuemJSONValue val) {
  return qbuem_json_size(val) == 0 ? 1 : 0;
}

QbuemJSONValue qbuem_json_get_idx(QbuemJSONValue val, size_t idx) {
  auto v = to_cpp(val);
  try { return from_cpp(v[static_cast<int>(idx)]); } catch (...) { return invalid_val(); }
}

QbuemJSONValue qbuem_json_get_key(QbuemJSONValue val, const char* key) {
  if (!key) return invalid_val();
  auto v = to_cpp(val);
  try { return from_cpp(v[key]); } catch (...) { return invalid_val(); }
}

QbuemJSONValue qbuem_json_at_path(QbuemJSONValue val, const char* pointer) {
  if (!pointer) return invalid_val();
  auto v = to_cpp(val);
  try { return from_cpp(v.at(std::string_view(pointer))); } catch (...) { return invalid_val(); }
}

// ── Object key iteration ──────────────────────────────────────────────────────

struct QbuemJSONIter_ {
  qbuem::Value::ObjectIterator current;
  qbuem::Value::ObjectIterator end;
  std::pair<std::string_view, qbuem::Value> last;
  bool started = false;
};

QbuemJSONIter* qbuem_json_iter_create(QbuemJSONValue obj) {
  auto v = to_cpp(obj);
  if (!v.is_object()) return nullptr;
  try {
    auto range = v.items();
    return new QbuemJSONIter_{range.begin(), range.end(), {}, false};
  } catch (...) { return nullptr; }
}

int qbuem_json_iter_next(QbuemJSONIter* it) {
  if (!it) return 0;
  if (!it->started) {
    it->started = true;
  } else {
    if (it->current == it->end) return 0;
    ++it->current;
  }
  if (it->current == it->end) return 0;
  it->last = *(it->current);
  return 1;
}

const char* qbuem_json_iter_key(const QbuemJSONIter* it, size_t* out_len) {
  if (!it || !it->started || it->current == it->end) return nullptr;
  if (out_len) *out_len = it->last.first.size();
  return it->last.first.data();
}

QbuemJSONValue qbuem_json_iter_value(QbuemJSONIter* it) {
  if (!it || !it->started || it->current == it->end) return invalid_val();
  return from_cpp(it->last.second);
}

void qbuem_json_iter_destroy(QbuemJSONIter* it) {
  delete it;
}

// ── Serialization ─────────────────────────────────────────────────────────────

const char* qbuem_json_dump(QbuemJSONDocument* doc, QbuemJSONValue val, size_t* out_len) {
  if (!doc) return nullptr;
  auto v = to_cpp(val);
  try {
    doc->dump_buf = v.dump();
    if (out_len) *out_len = doc->dump_buf.size();
    return doc->dump_buf.c_str();
  } catch (...) { return nullptr; }
}

const char* qbuem_json_dump_pretty(QbuemJSONDocument* doc, QbuemJSONValue val,
                               int indent, size_t* out_len) {
  if (!doc) return nullptr;
  auto v = to_cpp(val);
  try {
    doc->dump_buf = v.dump(indent);
    if (out_len) *out_len = doc->dump_buf.size();
    return doc->dump_buf.c_str();
  } catch (...) { return nullptr; }
}

// ── Mutation ──────────────────────────────────────────────────────────────────

void qbuem_json_set_int(QbuemJSONValue val, int64_t v) {
  auto cv = to_cpp(val);
  if (cv.is_valid()) try { cv.set(v); } catch (...) {}
}
void qbuem_json_set_double(QbuemJSONValue val, double v) {
  auto cv = to_cpp(val);
  if (cv.is_valid()) try { cv.set(v); } catch (...) {}
}
void qbuem_json_set_string(QbuemJSONValue val, const char* str, size_t len) {
  auto cv = to_cpp(val);
  if (cv.is_valid() && str)
    try { cv.set(std::string_view(str, len)); } catch (...) {}
}
void qbuem_json_set_null(QbuemJSONValue val) {
  auto cv = to_cpp(val);
  if (cv.is_valid()) try { cv.set(nullptr); } catch (...) {}
}
void qbuem_json_set_bool(QbuemJSONValue val, int b) {
  auto cv = to_cpp(val);
  if (cv.is_valid()) try { cv.set(b != 0); } catch (...) {}
}
void qbuem_json_insert_raw(QbuemJSONValue val, const char* key, const char* raw_json) {
  auto cv = to_cpp(val);
  if (cv.is_valid() && key && raw_json)
    try {
      cv.insert_json(std::string_view(key), std::string_view(raw_json));
    } catch (...) {}
}
void qbuem_json_append_raw(QbuemJSONValue val, const char* raw_json) {
  auto cv = to_cpp(val);
  if (cv.is_valid() && raw_json)
    try {
      cv.push_back_json(std::string_view(raw_json));
    } catch (...) {}
}
void qbuem_json_erase_key(QbuemJSONValue val, const char* key) {
  auto cv = to_cpp(val);
  if (cv.is_valid() && key)
    try { cv.erase(std::string_view(key)); } catch (...) {}
}
void qbuem_json_erase_idx(QbuemJSONValue val, size_t idx) {
  auto cv = to_cpp(val);
  if (cv.is_valid())
    try { cv.erase(static_cast<int>(idx)); } catch (...) {}
}
