package qbuem

/*
#cgo CFLAGS: -I../c
#cgo LDFLAGS: -L../../build/bindings/c -lqbuem_json_c
#include <stdlib.h>
#include "qbuem_json_c.h"
*/
import "C"
import (
	"fmt"
	"runtime"
	"unsafe"
)

// Document wraps QbuemJSONDocument
type Document struct {
	ptr *C.QbuemJSONDocument
}

// NewDocument creates a new JSON document
func NewDocument() *Document {
	d := &Document{ptr: C.qbuem_json_doc_create()}
	runtime.SetFinalizer(d, func(d *Document) {
		C.qbuem_json_doc_destroy(d.ptr)
	})
	return d
}

// Parse parses JSON string into the document
func (d *Document) Parse(json string) error {
	cStr := C.CString(json)
	defer C.free(unsafe.Pointer(cStr))

	v := C.qbuem_json_parse(d.ptr, cStr, C.size_t(len(json)))
	if C.qbuem_json_is_valid(v) == 0 {
		return fmt.Errorf("qbuem-json: parse error: %s", C.GoString(C.qbuem_json_last_error(d.ptr)))
	}
	return nil
}

// Root returns the root value of the document
func (d *Document) Root() Value {
	return Value{handle: C.qbuem_json_doc_get_root(d.ptr), doc: d}
}

// Value wraps QbuemJSONValue and maintains a reference to its parent Document to prevent GC
// and for operations that require the document context (like Serializing).
type Value struct {
	handle C.QbuemJSONValue
	doc    *Document
}

// IsValid checks if the value is valid
func (v Value) IsValid() bool { return C.qbuem_json_is_valid(v.handle) != 0 }

// IsNull checks if the value is null
func (v Value) IsNull() bool {
	return C.qbuem_json_type(v.handle) == C.QBUEM_JSON_TYPE_NULL
}

// IsBool checks if the value is a boolean
func (v Value) IsBool() bool {
	return C.qbuem_json_type(v.handle) == C.QBUEM_JSON_TYPE_BOOL
}

// IsInt checks if the value is an integer
func (v Value) IsInt() bool {
	return C.qbuem_json_type(v.handle) == C.QBUEM_JSON_TYPE_INT
}

// IsDouble checks if the value is a double
func (v Value) IsDouble() bool {
	return C.qbuem_json_type(v.handle) == C.QBUEM_JSON_TYPE_DOUBLE
}

// IsString checks if the value is a string
func (v Value) IsString() bool {
	return C.qbuem_json_type(v.handle) == C.QBUEM_JSON_TYPE_STRING
}

// IsArray checks if the value is an array
func (v Value) IsArray() bool {
	return C.qbuem_json_type(v.handle) == C.QBUEM_JSON_TYPE_ARRAY
}

// IsObject checks if the value is an object
func (v Value) IsObject() bool {
	return C.qbuem_json_type(v.handle) == C.QBUEM_JSON_TYPE_OBJECT
}

// Size returns the size of the array or object
func (v Value) Size() int { return int(C.qbuem_json_size(v.handle)) }

// Get returns the value associated with the key
func (v Value) Get(key string) Value {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))
	return Value{handle: C.qbuem_json_get_key(v.handle, cKey), doc: v.doc}
}

// Index returns the value at the specified index
func (v Value) Index(idx int) Value {
	return Value{handle: C.qbuem_json_get_idx(v.handle, C.size_t(idx)), doc: v.doc}
}

// At returns the value at the specified JSON pointer path
func (v Value) At(path string) Value {
	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))
	return Value{handle: C.qbuem_json_at_path(v.handle, cPath), doc: v.doc}
}

// AsBool returns the value as a boolean
func (v Value) AsBool() bool { return bool(C.qbuem_json_as_bool(v.handle) != 0) }

// AsInt returns the value as an int64
func (v Value) AsInt() int64 { return int64(C.qbuem_json_as_int(v.handle)) }

// AsDouble returns the value as a float64
func (v Value) AsDouble() float64 { return float64(C.qbuem_json_as_double(v.handle)) }

// AsString returns the value as a string
func (v Value) AsString() string {
	var n C.size_t
	cStr := C.qbuem_json_as_string(v.handle, &n)
	if cStr == nil {
		return ""
	}
	return C.GoStringN(cStr, C.int(n))
}

// Dump serializes the value to a JSON string
func (v Value) Dump(indent int) string {
	if v.doc == nil || v.doc.ptr == nil {
		return ""
	}
	var n C.size_t
	var cStr *C.char
	if indent > 0 {
		cStr = C.qbuem_json_dump_pretty(v.doc.ptr, v.handle, C.int(indent), &n)
	} else {
		cStr = C.qbuem_json_dump(v.doc.ptr, v.handle, &n)
	}
	if cStr == nil {
		return ""
	}
	return C.GoStringN(cStr, C.int(n))
}

// ToInterface converts the value and its children to native Go types
func (v Value) ToInterface() interface{} {
	if !v.IsValid() {
		return nil
	}
	switch C.qbuem_json_type(v.handle) {
	case C.QBUEM_JSON_TYPE_NULL:
		return nil
	case C.QBUEM_JSON_TYPE_BOOL:
		return v.AsBool()
	case C.QBUEM_JSON_TYPE_INT:
		return v.AsInt()
	case C.QBUEM_JSON_TYPE_DOUBLE:
		return v.AsDouble()
	case C.QBUEM_JSON_TYPE_STRING:
		return v.AsString()
	case C.QBUEM_JSON_TYPE_ARRAY:
		res := make([]interface{}, v.Size())
		for i := 0; i < v.Size(); i++ {
			res[i] = v.Index(i).ToInterface()
		}
		return res
	case C.QBUEM_JSON_TYPE_OBJECT:
		res := make(map[string]interface{})
		it := v.NewIterator()
		if it != nil {
			defer it.Close()
			for it.Next() {
				res[it.Key()] = it.Value().ToInterface()
			}
		}
		return res
	default:
		return nil
	}
}

// Iterator wraps QbuemJSONIter
type Iterator struct {
	ptr *C.QbuemJSONIter
	doc *Document
}

// NewIterator creates a new iterator over an object
func (v Value) NewIterator() *Iterator {
	it := C.qbuem_json_iter_create(v.handle)
	if it == nil {
		return nil
	}
	return &Iterator{ptr: it, doc: v.doc}
}

// Next advances the iterator and returns true if the current entry is valid
func (it *Iterator) Next() bool {
	return bool(C.qbuem_json_iter_next(it.ptr) != 0)
}

// Key returns the key of the current entry
func (it *Iterator) Key() string {
	var n C.size_t
	s := C.qbuem_json_iter_key(it.ptr, &n)
	return C.GoStringN(s, C.int(n))
}

// Value returns the value of the current entry
func (it *Iterator) Value() Value {
	return Value{handle: C.qbuem_json_iter_value(it.ptr), doc: it.doc}
}

// Close destroys the iterator
func (it *Iterator) Close() {
	if it.ptr != nil {
		C.qbuem_json_iter_destroy(it.ptr)
		it.ptr = nil
	}
}

// ── Mutation ─────────────────────────────────────────────────────────────────

// Set updates a scalar value. Supports bool, int, float64, string, and nil.
func (v Value) Set(val interface{}) {
	if val == nil {
		C.qbuem_json_set_null(v.handle)
		return
	}
	switch tv := val.(type) {
	case bool:
		b := 0
		if tv {
			b = 1
		}
		C.qbuem_json_set_bool(v.handle, C.int(b))
	case int:
		C.qbuem_json_set_int(v.handle, C.int64_t(tv))
	case int64:
		C.qbuem_json_set_int(v.handle, C.int64_t(tv))
	case float64:
		C.qbuem_json_set_double(v.handle, C.double(tv))
	case string:
		cStr := C.CString(tv)
		defer C.free(unsafe.Pointer(cStr))
		C.qbuem_json_set_string(v.handle, cStr, C.size_t(len(tv)))
	}
}

// Insert adds a key-value pair to an object. The value is automatically serialized if supported.
func (v Value) Insert(key string, val interface{}) {
	if s, ok := val.(string); ok {
		v.InsertRaw(key, fmt.Sprintf("\"%s\"", s))
		return
	}
	v.InsertRaw(key, fmt.Sprintf("%v", val))
}

// InsertRaw adds a key-value pair to an object using pre-serialized JSON content.
func (v Value) InsertRaw(key string, rawJSON string) {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))
	cRaw := C.CString(rawJSON)
	defer C.free(unsafe.Pointer(cRaw))
	C.qbuem_json_insert_raw(v.handle, cKey, cRaw)
}

// AppendRaw appends a pre-serialized JSON value to an array.
func (v Value) AppendRaw(rawJSON string) {
	cRaw := C.CString(rawJSON)
	defer C.free(unsafe.Pointer(cRaw))
	C.qbuem_json_append_raw(v.handle, cRaw)
}

// Erase removes a key (from object) or an index (from array).
func (v Value) Erase(keyOrIdx interface{}) {
	switch tv := keyOrIdx.(type) {
	case string:
		cKey := C.CString(tv)
		defer C.free(unsafe.Pointer(cKey))
		C.qbuem_json_erase_key(v.handle, cKey)
	case int:
		C.qbuem_json_erase_idx(v.handle, C.size_t(tv))
	}
}
