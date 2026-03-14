package qbuem

import (
	"reflect"
	"unsafe"
)

/*
#include <stdlib.h>
#include "qbuem_json_c.h"
*/
import "C"

// Marshal returns the JSON encoding of v.
// It uses qbuem-json mutation API to build the document.
func Marshal(v interface{}) ([]byte, error) {
	rv := reflect.ValueOf(v)
	doc := NewDocument()

	// Initialize document with the correct top-level container
	initJSON := "null"
	kind := rv.Kind()
	if kind == reflect.Ptr || kind == reflect.Interface {
		if !rv.IsNil() {
			kind = rv.Elem().Kind()
		}
	}

	switch kind {
	case reflect.Struct, reflect.Map:
		initJSON = "{}"
	case reflect.Slice, reflect.Array:
		initJSON = "[]"
	}

	if err := doc.Parse(initJSON); err != nil {
		return nil, err
	}
	root := doc.Root()
	if err := encodeValue(root, rv); err != nil {
		return nil, err
	}
	return []byte(root.Dump(0)), nil
}

func encodeValue(qv Value, rv reflect.Value) error {
	if !rv.IsValid() {
		C.qbuem_json_set_null(qv.handle)
		return nil
	}

	switch rv.Kind() {
	case reflect.Ptr, reflect.Interface:
		if rv.IsNil() {
			C.qbuem_json_set_null(qv.handle)
			return nil
		}
		return encodeValue(qv, rv.Elem())
	case reflect.Struct:
		return encodeStruct(qv, rv)
	case reflect.Map:
		return encodeMap(qv, rv)
	case reflect.Slice, reflect.Array:
		return encodeSlice(qv, rv)
	case reflect.String:
		s := rv.String()
		cs := C.CString(s)
		defer C.free(unsafe.Pointer(cs))
		C.qbuem_json_set_string(qv.handle, cs, C.size_t(len(s)))
		return nil
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		C.qbuem_json_set_int(qv.handle, C.int64_t(rv.Int()))
		return nil
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		C.qbuem_json_set_int(qv.handle, C.int64_t(rv.Uint()))
		return nil
	case reflect.Float32, reflect.Float64:
		C.qbuem_json_set_double(qv.handle, C.double(rv.Float()))
		return nil
	case reflect.Bool:
		b := 0
		if rv.Bool() {
			b = 1
		}
		C.qbuem_json_set_bool(qv.handle, C.int(b))
		return nil
	}
	return nil
}

func encodeStruct(qv Value, rv reflect.Value) error {
	t := rv.Type()
	for i := 0; i < rv.NumField(); i++ {
		f := t.Field(i)
		if f.PkgPath != "" { // private field
			continue
		}
		tag := f.Tag.Get("json")
		if tag == "-" {
			continue
		}
		name := f.Name
		if tag != "" {
			name = tag
		}

		fv, err := Marshal(rv.Field(i).Interface())
		if err != nil {
			return err
		}
		qv.InsertRaw(name, string(fv))
	}
	return nil
}

func encodeMap(qv Value, rv reflect.Value) error {
	for _, key := range rv.MapKeys() {
		if key.Kind() != reflect.String {
			continue
		}
		fv, err := Marshal(rv.MapIndex(key).Interface())
		if err != nil {
			return err
		}
		qv.InsertRaw(key.String(), string(fv))
	}
	return nil
}

func encodeSlice(qv Value, rv reflect.Value) error {
	for i := 0; i < rv.Len(); i++ {
		fv, err := Marshal(rv.Index(i).Interface())
		if err != nil {
			return err
		}
		qv.AppendRaw(string(fv))
	}
	return nil
}
