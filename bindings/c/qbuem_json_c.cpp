/**
 * qbuem_json_c.cpp — qbuem-json C API implementation
 */

#include "qbuem_json_c.h"
#include <qbuem_json/qbuem_json.hpp>

#include <cstring>
#include <string>
#include <vector>

// ── Internal structures ───────────────────────────────────────────────────────

struct BJSONValue_ {
  qbuem::Value val;
  bool         valid = false;
};

struct BJSONDocument_ {
  qbuem::Document          doc;
  std::string              last_error;
  std::string              dump_buf;
  std::vector<BJSONValue_*> value_pool;  // all Values owned by this document

  BJSONValue_* alloc(qbuem::Value v) {
    auto* bv = new BJSONValue_{v, true};
    value_pool.push_back(bv);
    return bv;
  }

  BJSONValue_* invalid_val() {
    auto* bv = new BJSONValue_{qbuem::Value{}, false};
    value_pool.push_back(bv);
    return bv;
  }

  void clear_pool() {
    for (auto* p : value_pool) delete p;
    value_pool.clear();
  }

  ~BJSONDocument_() { clear_pool(); }
};

// ── Document lifecycle ────────────────────────────────────────────────────────

BJSONDocument* bjson_doc_create() {
  try { return new BJSONDocument_{}; }
  catch (...) { return nullptr; }
}

void bjson_doc_destroy(BJSONDocument* doc) {
  delete doc;
}

// ── Parsing ───────────────────────────────────────────────────────────────────

BJSONValue* bjson_parse(BJSONDocument* doc,
                        const char* json_data, size_t json_len) {
  if (!doc || !json_data) return nullptr;
  try {
    doc->clear_pool();
    doc->last_error.clear();
    qbuem::Value root = qbuem::parse(doc->doc,
                                     std::string_view(json_data, json_len));
    return doc->alloc(root);
  } catch (const std::exception& e) {
    doc->last_error = e.what();
    return nullptr;
  }
}

BJSONValue* bjson_parse_strict(BJSONDocument* doc,
                               const char* json_data, size_t json_len) {
  if (!doc || !json_data) return nullptr;
  try {
    doc->clear_pool();
    doc->last_error.clear();
    qbuem::Value root = qbuem::parse_strict(doc->doc,
                                             std::string_view(json_data, json_len));
    return doc->alloc(root);
  } catch (const std::exception& e) {
    doc->last_error = e.what();
    return nullptr;
  }
}

const char* bjson_last_error(const BJSONDocument* doc) {
  if (!doc) return "null document";
  return doc->last_error.c_str();
}

// ── Value introspection ───────────────────────────────────────────────────────

BJSONType bjson_type(const BJSONValue* val) {
  if (!val || !val->valid) return BJSON_TYPE_INVALID;
  const auto& v = val->val;
  if (v.is_null())   return BJSON_TYPE_NULL;
  if (v.is_bool())   return BJSON_TYPE_BOOL;
  if (v.is_int())    return BJSON_TYPE_INT;
  if (v.is_double()) return BJSON_TYPE_DOUBLE;
  if (v.is_string()) return BJSON_TYPE_STRING;
  if (v.is_array())  return BJSON_TYPE_ARRAY;
  if (v.is_object()) return BJSON_TYPE_OBJECT;
  return BJSON_TYPE_INVALID;
}

const char* bjson_type_name(const BJSONValue* val) {
  if (!val || !val->valid) return "invalid";
  return val->val.type_name().data();
}

int bjson_is_valid(const BJSONValue* val) {
  return (val && val->valid && val->val.is_valid()) ? 1 : 0;
}

// ── Scalar extraction ─────────────────────────────────────────────────────────

int bjson_as_bool(const BJSONValue* val) {
  if (!val || !val->valid) return 0;
  try { return val->val.as<bool>() ? 1 : 0; } catch (...) { return 0; }
}

int64_t bjson_as_int(const BJSONValue* val) {
  if (!val || !val->valid) return 0;
  try { return val->val.as<int64_t>(); } catch (...) { return 0; }
}

double bjson_as_double(const BJSONValue* val) {
  if (!val || !val->valid) return 0.0;
  try {
    if (val->val.is_int()) return static_cast<double>(val->val.as<int64_t>());
    return val->val.as<double>();
  } catch (...) { return 0.0; }
}

const char* bjson_as_string(const BJSONValue* val, size_t* out_len) {
  if (!val || !val->valid) return nullptr;
  try {
    auto sv = val->val.as<std::string_view>();
    if (out_len) *out_len = sv.size();
    return sv.data();
  } catch (...) { return nullptr; }
}

// ── Container access ──────────────────────────────────────────────────────────

size_t bjson_size(const BJSONValue* val) {
  if (!val || !val->valid) return 0;
  try { return val->val.size(); } catch (...) { return 0; }
}

int bjson_empty(const BJSONValue* val) {
  return bjson_size(val) == 0 ? 1 : 0;
}

BJSONValue* bjson_get_idx(BJSONDocument* doc,
                          const BJSONValue* val, size_t idx) {
  if (!doc || !val || !val->valid) return doc ? doc->invalid_val() : nullptr;
  try {
    qbuem::Value child = val->val[static_cast<int>(idx)];
    if (!child.is_valid()) return doc->invalid_val();
    return doc->alloc(child);
  } catch (...) { return doc->invalid_val(); }
}

BJSONValue* bjson_get_key(BJSONDocument* doc,
                          const BJSONValue* val, const char* key) {
  if (!doc || !val || !val->valid || !key)
    return doc ? doc->invalid_val() : nullptr;
  try {
    auto opt = val->val.find(std::string_view(key));
    if (!opt) return doc->invalid_val();
    return doc->alloc(*opt);
  } catch (...) { return doc->invalid_val(); }
}

BJSONValue* bjson_at_path(BJSONDocument* doc,
                          const BJSONValue* val, const char* pointer) {
  if (!doc || !val || !val->valid || !pointer)
    return doc ? doc->invalid_val() : nullptr;
  try {
    qbuem::Value v = val->val.at(std::string_view(pointer));
    if (!v.is_valid()) return doc->invalid_val();
    return doc->alloc(v);
  } catch (...) { return doc->invalid_val(); }
}

// ── Object key iteration ──────────────────────────────────────────────────────

struct BJSONIter_ {
  BJSONDocument*  doc;
  qbuem::Value    obj_val;  // keep the object value alive
  bool            started = false;

  // We use a cached items list for simplicity
  std::vector<std::pair<std::string, qbuem::Value>> entries;
  size_t pos = 0;

  explicit BJSONIter_(BJSONDocument* d, qbuem::Value v) : doc(d), obj_val(v) {
    for (const auto& [k, val] : v.items())
      entries.emplace_back(std::string(k), val);
  }
};

BJSONIter* bjson_iter_create(BJSONDocument* doc, const BJSONValue* obj) {
  if (!doc || !obj || !obj->valid || !obj->val.is_object()) return nullptr;
  try { return new BJSONIter_{doc, obj->val}; }
  catch (...) { return nullptr; }
}

int bjson_iter_next(BJSONIter* it) {
  if (!it) return 0;
  if (!it->started) { it->started = true; }
  else { ++it->pos; }
  return (it->pos < it->entries.size()) ? 1 : 0;
}

const char* bjson_iter_key(const BJSONIter* it, size_t* out_len) {
  if (!it || it->pos >= it->entries.size()) return nullptr;
  const auto& k = it->entries[it->pos].first;
  if (out_len) *out_len = k.size();
  return k.c_str();
}

BJSONValue* bjson_iter_value(BJSONIter* it) {
  if (!it || it->pos >= it->entries.size()) return nullptr;
  return it->doc->alloc(it->entries[it->pos].second);
}

void bjson_iter_destroy(BJSONIter* it) { delete it; }

// ── Serialization ─────────────────────────────────────────────────────────────

const char* bjson_dump(BJSONDocument* doc, const BJSONValue* val, size_t* out_len) {
  if (!doc || !val || !val->valid) return nullptr;
  try {
    doc->dump_buf = val->val.dump();
    if (out_len) *out_len = doc->dump_buf.size();
    return doc->dump_buf.c_str();
  } catch (...) { return nullptr; }
}

const char* bjson_dump_pretty(BJSONDocument* doc, const BJSONValue* val,
                              int indent, size_t* out_len) {
  if (!doc || !val || !val->valid) return nullptr;
  try {
    doc->dump_buf = val->val.dump(indent);
    if (out_len) *out_len = doc->dump_buf.size();
    return doc->dump_buf.c_str();
  } catch (...) { return nullptr; }
}

// ── Mutation ──────────────────────────────────────────────────────────────────

void bjson_set_int(BJSONValue* val, int64_t v) {
  if (val && val->valid) try { val->val.set(v); } catch (...) {}
}
void bjson_set_double(BJSONValue* val, double v) {
  if (val && val->valid) try { val->val.set(v); } catch (...) {}
}
void bjson_set_string(BJSONValue* val, const char* str, size_t len) {
  if (val && val->valid && str)
    try { val->val.set(std::string_view(str, len)); } catch (...) {}
}
void bjson_set_null(BJSONValue* val) {
  if (val && val->valid) try { val->val.set(nullptr); } catch (...) {}
}
void bjson_set_bool(BJSONValue* val, int b) {
  if (val && val->valid) try { val->val.set(b != 0); } catch (...) {}
}
void bjson_insert_raw(BJSONValue* val, const char* key, const char* raw_json) {
  if (val && val->valid && key && raw_json)
    try {
      val->val.insert_json(std::string_view(key), std::string_view(raw_json));
    } catch (...) {}
}
void bjson_erase_key(BJSONValue* val, const char* key) {
  if (val && val->valid && key)
    try { val->val.erase(std::string_view(key)); } catch (...) {}
}
void bjson_erase_idx(BJSONValue* val, size_t idx) {
  if (val && val->valid)
    try { val->val.erase(static_cast<int>(idx)); } catch (...) {}
}
