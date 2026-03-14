package qbuem

import (
	"fmt"
	"reflect"
)

// Unmarshal parses the JSON-encoded data and stores the result in the value pointed to by v.
func Unmarshal(data []byte, v interface{}) error {
	doc := NewDocument()
	if err := doc.Parse(string(data)); err != nil {
		return err
	}
	root := doc.Root()
	if !root.IsValid() {
		return fmt.Errorf("qbuem-json: invalid json")
	}

	rv := reflect.ValueOf(v)
	if rv.Kind() != reflect.Ptr || rv.IsNil() {
		return fmt.Errorf("qbuem-json: Unmarshal requires a non-nil pointer")
	}

	return decodeValue(root, rv.Elem())
}

func decodeValue(qv Value, rv reflect.Value) error {
	if !qv.IsValid() {
		return nil
	}

	switch rv.Kind() {
	case reflect.Interface:
		if rv.NumMethod() == 0 {
			// Empty interface, decide what to put based on qv type
			return decodeToInterface(qv, rv)
		}
	case reflect.Struct:
		return decodeStruct(qv, rv)
	case reflect.Slice:
		return decodeSlice(qv, rv)
	case reflect.Map:
		return decodeMap(qv, rv)
	case reflect.Ptr:
		if rv.IsNil() {
			rv.Set(reflect.New(rv.Type().Elem()))
		}
		return decodeValue(qv, rv.Elem())
	case reflect.String:
		if qv.IsString() {
			rv.SetString(qv.AsString())
		}
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		if qv.IsInt() {
			rv.SetInt(qv.AsInt())
		}
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		if qv.IsInt() {
			rv.SetUint(uint64(qv.AsInt()))
		}
	case reflect.Float32, reflect.Float64:
		if qv.IsDouble() || qv.IsInt() {
			rv.SetFloat(qv.AsDouble())
		}
	case reflect.Bool:
		if qv.IsBool() {
			rv.SetBool(qv.AsBool())
		}
	}
	return nil
}

func decodeStruct(qv Value, rv reflect.Value) error {
	if !qv.IsObject() {
		return nil
	}

	t := rv.Type()
	for i := 0; i < rv.NumField(); i++ {
		field := t.Field(i)
		if field.PkgPath != "" { // Skip unexported fields
			continue
		}

		name := field.Name
		tag := field.Tag.Get("json")
		if tag == "-" {
			continue
		}
		if tag != "" {
			name = tag // Simplified: just use the tag name
		}

		fValue := qv.Get(name)
		if fValue.IsValid() {
			if err := decodeValue(fValue, rv.Field(i)); err != nil {
				return err
			}
		}
	}
	return nil
}

func decodeSlice(qv Value, rv reflect.Value) error {
	if !qv.IsArray() {
		return nil
	}

	size := qv.Size()
	slice := reflect.MakeSlice(rv.Type(), size, size)
	for i := 0; i < size; i++ {
		if err := decodeValue(qv.Index(i), slice.Index(i)); err != nil {
			return err
		}
	}
	rv.Set(slice)
	return nil
}

func decodeMap(qv Value, rv reflect.Value) error {
	if !qv.IsObject() {
		return nil
	}

	t := rv.Type()
	if t.Key().Kind() != reflect.String {
		return fmt.Errorf("qbuem-json: expect map with string keys")
	}

	if rv.IsNil() {
		rv.Set(reflect.MakeMap(t))
	}

	it := qv.NewIterator()
	if it == nil {
		return nil
	}
	defer it.Close()

	for it.Next() {
		key := it.Key()
		val := it.Value()
		
		elem := reflect.New(t.Elem()).Elem()
		if err := decodeValue(val, elem); err != nil {
			return err
		}
		rv.SetMapIndex(reflect.ValueOf(key), elem)
	}
	
	return nil
}

func decodeToInterface(qv Value, rv reflect.Value) error {
	var val interface{}
	if qv.IsString() {
		val = qv.AsString()
	} else if qv.IsInt() {
		val = qv.AsInt()
	} else if qv.IsDouble() {
		val = qv.AsDouble()
	} else if qv.IsBool() {
		val = qv.AsBool()
	} else if qv.IsNull() {
		val = nil
	} else if qv.IsArray() {
		res := make([]interface{}, qv.Size())
		for i := 0; i < qv.Size(); i++ {
			var item interface{}
			if err := decodeToInterface(qv.Index(i), reflect.ValueOf(&item).Elem()); err != nil {
				return err
			}
			res[i] = item
		}
		val = res
	} else if qv.IsObject() {
		res := make(map[string]interface{})
		it := qv.NewIterator()
		if it != nil {
			defer it.Close()
			for it.Next() {
				var item interface{}
				if err := decodeToInterface(it.Value(), reflect.ValueOf(&item).Elem()); err != nil {
					return err
				}
				res[it.Key()] = item
			}
		}
		val = res
	}
	
	if val != nil {
		rv.Set(reflect.ValueOf(val))
	}
	return nil
}
