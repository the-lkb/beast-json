/**
 * qbuem_json_c.h — qbuem-json C API
 *
 * A pure C interface to the qbuem-json C++20 library.
 * Designed for use with ctypes (Python), cffi, or any C-compatible FFI.
 *
 * Lifetime model:
 *   1. qbuem_json_doc_create()       → allocate a document context
 *   2. qbuem_json_parse(doc, json)   → parse into the document, get root Value
 *   3. Access values via qbuem_json_value_* functions (Value handles are POD)
 *   4. qbuem_json_doc_destroy(doc)   → release all memory
 *
 * A document context is NOT thread-safe; use one per thread.
 * Values are only valid while the document is alive and has not been re-parsed.
 *
 * Error handling: functions that can fail return INVALID types or NULL pointers.
 * Use qbuem_json_last_error() to retrieve the last error message.
 */

#ifndef QBUEM_JSON_C_H
#define QBUEM_JSON_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque handle types ─────────────────────────────────────────────────── */

typedef struct QbuemJSONDocument_ QbuemJSONDocument;

/**
 * QbuemJSONValue — Transparent POD handle to a JSON node.
 * 
 * This is a lightweight view into the document's tape. It does NOT require 
 * manual allocation or deallocation. It can be passed by value across FFI.
 */
typedef struct {
  void*    _internal_doc; /**< Internal: pointer to the DocumentState */
  uint32_t _internal_idx; /**< Internal: index into the tape */
} QbuemJSONValue;

/* ── Value type enumeration ─────────────────────────────────────────────────── */

typedef enum {
  QBUEM_JSON_TYPE_INVALID = 0,  /**< missing / out-of-range / default */
  QBUEM_JSON_TYPE_NULL    = 1,  /**< JSON null   */
  QBUEM_JSON_TYPE_BOOL    = 2,  /**< JSON boolean */
  QBUEM_JSON_TYPE_INT     = 3,  /**< JSON integer (fits in int64) */
  QBUEM_JSON_TYPE_DOUBLE  = 4,  /**< JSON floating-point number */
  QBUEM_JSON_TYPE_STRING  = 5,  /**< JSON string */
  QBUEM_JSON_TYPE_ARRAY   = 6,  /**< JSON array  */
  QBUEM_JSON_TYPE_OBJECT  = 7,  /**< JSON object */
} QbuemJSONType;

/* ── Document lifecycle ─────────────────────────────────────────────────────── */

/** Allocate a new document context. Returns NULL on OOM. */
QbuemJSONDocument* qbuem_json_doc_create(void);

/** Free a document context and all associated memory. */
void qbuem_json_doc_destroy(QbuemJSONDocument* doc);

/* ── Parsing ────────────────────────────────────────────────────────────────── */

/**
 * Parse \p json_data (length \p json_len) into \p doc.
 * Returns the root QbuemJSONValue on success. On failure, returns a value 
 * where qbuem_json_is_valid() is false.
 *
 * The JSON data must remain alive for the lifetime of the document
 * (the parser is zero-copy for string content).
 */
QbuemJSONValue qbuem_json_parse(QbuemJSONDocument* doc,
                       const char*    json_data,
                       size_t         json_len);

/**
 * Like qbuem_json_parse() but enforces strict RFC 8259 compliance.
 */
QbuemJSONValue qbuem_json_parse_strict(QbuemJSONDocument* doc,
                               const char*    json_data,
                               size_t         json_len);

/**
 * Return the last error string for \p doc (valid until next call).
 * Returns "" if no error has occurred.
 */
const char* qbuem_json_last_error(const QbuemJSONDocument* doc);

/**
 * Return the root value of the document.
 */
QbuemJSONValue qbuem_json_doc_get_root(const QbuemJSONDocument* doc);


/* ── Value introspection ────────────────────────────────────────────────────── */

/** Return the type of \p val. */
QbuemJSONType qbuem_json_type(QbuemJSONValue val);

/** Return the human-readable type name ("null","bool","int","double","string","array","object","invalid"). */
const char* qbuem_json_type_name(QbuemJSONValue val);

/** Return non-zero if \p val is valid. */
int qbuem_json_is_valid(QbuemJSONValue val);

/* ── Scalar extraction ──────────────────────────────────────────────────────── */

/** Extract bool. Returns 0/1. On type mismatch returns 0. */
int qbuem_json_as_bool(QbuemJSONValue val);

/** Extract int64. On type mismatch returns 0. */
int64_t qbuem_json_as_int(QbuemJSONValue val);

/** Extract double. Converts int to double if needed. On mismatch returns 0.0. */
double qbuem_json_as_double(QbuemJSONValue val);

/**
 * Extract string. Sets *out_len to the byte count (UTF-8, not null-terminated
 * in the original source).  Returns NULL on type mismatch.
 * The pointer is valid while the document is alive.
 */
const char* qbuem_json_as_string(QbuemJSONValue val, size_t* out_len);

/* ── Container access ───────────────────────────────────────────────────────── */

/** Return the number of elements (array) or key-value pairs (object). */
size_t qbuem_json_size(QbuemJSONValue val);

/** Return non-zero if \p val has zero elements. */
int qbuem_json_empty(QbuemJSONValue val);

/**
 * Array index access.  Returns an invalid QbuemJSONValue if out of range.
 */
QbuemJSONValue qbuem_json_get_idx(QbuemJSONValue val, size_t idx);

/**
 * Object key access.  Returns an invalid QbuemJSONValue if key absent.
 * \p key must be a null-terminated C string.
 */
QbuemJSONValue qbuem_json_get_key(QbuemJSONValue val, const char* key);

/**
 * RFC 6901 JSON Pointer access.  \p pointer is a null-terminated path like
 * "/users/0/name".  Returns an invalid value if the path does not exist.
 */
QbuemJSONValue qbuem_json_at_path(QbuemJSONValue val, const char* pointer);

/* ── Object key iteration ───────────────────────────────────────────────────── */

/**
 * Opaque iterator handle.  Use qbuem_json_iter_* functions to walk object entries.
 * Iterators are now zero-copy.
 */
typedef struct QbuemJSONIter_ QbuemJSONIter;

/** Create an iterator over the key-value pairs of an object value. */
QbuemJSONIter* qbuem_json_iter_create(QbuemJSONValue obj);

/** Advance the iterator. Returns non-zero if the current entry is valid. */
int qbuem_json_iter_next(QbuemJSONIter* it);

/** Get the key string of the current entry. Sets *out_len if non-NULL. */
const char* qbuem_json_iter_key(const QbuemJSONIter* it, size_t* out_len);

/** Get the value handle of the current entry. */
QbuemJSONValue qbuem_json_iter_value(QbuemJSONIter* it);

/** Destroy the iterator. */
void qbuem_json_iter_destroy(QbuemJSONIter* it);

/* ── Serialization ──────────────────────────────────────────────────────────── */

/**
 * Serialize \p val to JSON.  Sets *out_len to the byte count.
 * The returned buffer is owned by the document and valid until the next
 * dump call or re-parse.
 */
const char* qbuem_json_dump(QbuemJSONDocument* doc, QbuemJSONValue val, size_t* out_len);

/** Like qbuem_json_dump() but with indentation (0 = compact, 2 = two-space, etc.). */
const char* qbuem_json_dump_pretty(QbuemJSONDocument* doc, QbuemJSONValue val,
                               int indent, size_t* out_len);

/* ── Mutation ───────────────────────────────────────────────────────────────── */

/** Set a numeric integer value (overlay, original tape unchanged). */
void qbuem_json_set_int(QbuemJSONValue val, int64_t v);

/** Set a floating-point value. */
void qbuem_json_set_double(QbuemJSONValue val, double v);

/** Set a string value (\p str need not be null-terminated; length is \p len). */
void qbuem_json_set_string(QbuemJSONValue val, const char* str, size_t len);

/** Set null. */
void qbuem_json_set_null(QbuemJSONValue val);

/** Set bool. */
void qbuem_json_set_bool(QbuemJSONValue val, int b);

/**
 * Insert a key-value pair into an object value.
 * \p raw_json is a pre-serialized JSON string (e.g. "42" or "\"hello\"").
 */
void qbuem_json_insert_raw(QbuemJSONValue val, const char* key, const char* raw_json);

/**
 * Append a raw JSON value to an array value.
 */
void qbuem_json_append_raw(QbuemJSONValue val, const char* raw_json);

/** Remove a key from an object value. */
void qbuem_json_erase_key(QbuemJSONValue val, const char* key);

/** Remove an element at index \p idx from an array value. */
void qbuem_json_erase_idx(QbuemJSONValue val, size_t idx);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* QBUEM_JSON_C_H */
