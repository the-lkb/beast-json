/**
 * qbuem_json_c.h — qbuem-json C API
 *
 * A pure C interface to the qbuem-json C++20 library.
 * Designed for use with ctypes (Python), cffi, or any C-compatible FFI.
 *
 * Lifetime model:
 *   1. bjson_doc_create()       → allocate a document context
 *   2. bjson_parse(doc, json)   → parse into the document, get root Value
 *   3. Access values via bjson_value_* functions
 *   4. bjson_doc_destroy(doc)   → release all memory
 *
 * A document context is NOT thread-safe; use one per thread.
 * Values are only valid while the document is alive and has not been re-parsed.
 *
 * Error handling: functions that can fail return NULL or -1.
 * Use bjson_last_error() to retrieve the last error message.
 */

#ifndef QBUEM_JSON_C_H
#define QBUEM_JSON_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque handle types ────────────────────────────────────────────────────── */

typedef struct BJSONDocument_ BJSONDocument;
typedef struct BJSONValue_    BJSONValue;

/* ── Value type enumeration ─────────────────────────────────────────────────── */

typedef enum {
  BJSON_TYPE_INVALID = 0,  /**< missing / out-of-range / default */
  BJSON_TYPE_NULL    = 1,  /**< JSON null   */
  BJSON_TYPE_BOOL    = 2,  /**< JSON boolean */
  BJSON_TYPE_INT     = 3,  /**< JSON integer (fits in int64) */
  BJSON_TYPE_DOUBLE  = 4,  /**< JSON floating-point number */
  BJSON_TYPE_STRING  = 5,  /**< JSON string */
  BJSON_TYPE_ARRAY   = 6,  /**< JSON array  */
  BJSON_TYPE_OBJECT  = 7,  /**< JSON object */
} BJSONType;

/* ── Document lifecycle ─────────────────────────────────────────────────────── */

/** Allocate a new document context. Returns NULL on OOM. */
BJSONDocument* bjson_doc_create(void);

/** Free a document context and all associated memory. */
void bjson_doc_destroy(BJSONDocument* doc);

/* ── Parsing ────────────────────────────────────────────────────────────────── */

/**
 * Parse \p json_data (length \p json_len) into \p doc.
 * Returns the root BJSONValue* on success, NULL on parse error.
 * The returned value is owned by \p doc — do not free it.
 * Use bjson_last_error(doc) to get the error message on failure.
 *
 * The JSON data must remain alive for the lifetime of the document
 * (the parser is zero-copy for string content).
 */
BJSONValue* bjson_parse(BJSONDocument* doc,
                        const char*    json_data,
                        size_t         json_len);

/**
 * Like bjson_parse() but enforces strict RFC 8259 compliance.
 * Rejects: trailing commas, leading zeros, unescaped control chars, etc.
 */
BJSONValue* bjson_parse_strict(BJSONDocument* doc,
                               const char*    json_data,
                               size_t         json_len);

/**
 * Return the last error string for \p doc (valid until next call).
 * Returns "" if no error has occurred.
 */
const char* bjson_last_error(const BJSONDocument* doc);

/* ── Value introspection ────────────────────────────────────────────────────── */

/** Return the type of \p val. Returns BJSON_TYPE_INVALID if val is NULL. */
BJSONType bjson_type(const BJSONValue* val);

/** Return the human-readable type name ("null","bool","int","double","string","array","object","invalid"). */
const char* bjson_type_name(const BJSONValue* val);

/** Return non-zero if \p val is valid (non-NULL and not BJSON_TYPE_INVALID). */
int bjson_is_valid(const BJSONValue* val);

/* ── Scalar extraction ──────────────────────────────────────────────────────── */

/** Extract bool. Returns 0/1. On type mismatch returns 0. */
int bjson_as_bool(const BJSONValue* val);

/** Extract int64. On type mismatch returns 0. */
int64_t bjson_as_int(const BJSONValue* val);

/** Extract double. Converts int to double if needed. On mismatch returns 0.0. */
double bjson_as_double(const BJSONValue* val);

/**
 * Extract string. Sets *out_len to the byte count (UTF-8, not null-terminated
 * in the original source).  Returns NULL on type mismatch.
 * The pointer is valid while the document is alive.
 */
const char* bjson_as_string(const BJSONValue* val, size_t* out_len);

/* ── Container access ───────────────────────────────────────────────────────── */

/** Return the number of elements (array) or key-value pairs (object). */
size_t bjson_size(const BJSONValue* val);

/** Return non-zero if \p val has zero elements. */
int bjson_empty(const BJSONValue* val);

/**
 * Array index access.  Returns NULL (BJSON_TYPE_INVALID) if out of range.
 * The returned value is owned by the document.
 */
BJSONValue* bjson_get_idx(BJSONDocument* doc, const BJSONValue* val, size_t idx);

/**
 * Object key access.  Returns NULL (BJSON_TYPE_INVALID) if key absent.
 * \p key must be a null-terminated C string.
 */
BJSONValue* bjson_get_key(BJSONDocument* doc, const BJSONValue* val,
                          const char* key);

/**
 * RFC 6901 JSON Pointer access.  \p pointer is a null-terminated path like
 * "/users/0/name".  Returns NULL if the path does not exist.
 */
BJSONValue* bjson_at_path(BJSONDocument* doc, const BJSONValue* val,
                          const char* pointer);

/* ── Object key iteration ───────────────────────────────────────────────────── */

/**
 * Opaque iterator handle.  Use bjson_iter_* functions to walk object entries.
 * Call bjson_iter_destroy() when done.
 */
typedef struct BJSONIter_ BJSONIter;

/** Create an iterator over the key-value pairs of an object value. */
BJSONIter* bjson_iter_create(BJSONDocument* doc, const BJSONValue* obj);

/** Advance the iterator. Returns non-zero if the current entry is valid. */
int bjson_iter_next(BJSONIter* it);

/** Get the key string of the current entry. Sets *out_len if non-NULL. */
const char* bjson_iter_key(const BJSONIter* it, size_t* out_len);

/** Get the value of the current entry. Owned by the document. */
BJSONValue* bjson_iter_value(BJSONIter* it);

/** Destroy the iterator. */
void bjson_iter_destroy(BJSONIter* it);

/* ── Serialization ──────────────────────────────────────────────────────────── */

/**
 * Serialize \p val to JSON.  Sets *out_len to the byte count.
 * The returned buffer is owned by the document and valid until the next
 * dump call or re-parse.
 * Returns NULL on failure.
 */
const char* bjson_dump(BJSONDocument* doc, const BJSONValue* val,
                       size_t* out_len);

/** Like bjson_dump() but with indentation (0 = compact, 2 = two-space, etc.). */
const char* bjson_dump_pretty(BJSONDocument* doc, const BJSONValue* val,
                              int indent, size_t* out_len);

/* ── Mutation ───────────────────────────────────────────────────────────────── */

/** Set a numeric integer value (overlay, original tape unchanged). */
void bjson_set_int(BJSONValue* val, int64_t v);

/** Set a floating-point value. */
void bjson_set_double(BJSONValue* val, double v);

/** Set a string value (\p str need not be null-terminated; length is \p len). */
void bjson_set_string(BJSONValue* val, const char* str, size_t len);

/** Set null. */
void bjson_set_null(BJSONValue* val);

/** Set bool. */
void bjson_set_bool(BJSONValue* val, int b);

/**
 * Insert a key-value pair into an object value.
 * \p raw_json is a pre-serialized JSON string (e.g. "42" or "\"hello\"").
 */
void bjson_insert_raw(BJSONValue* val, const char* key, const char* raw_json);

/** Remove a key from an object value. */
void bjson_erase_key(BJSONValue* val, const char* key);

/** Remove an element at index \p idx from an array value. */
void bjson_erase_idx(BJSONValue* val, size_t idx);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* QBUEM_JSON_C_H */
