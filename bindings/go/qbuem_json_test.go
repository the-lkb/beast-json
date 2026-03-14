package qbuem

import (
	"strings"
	"testing"
)

func TestDOM(t *testing.T) {
	json := `{"name": "Alice", "age": 30, "scores": [1, 2, 3]}`
	doc := NewDocument()
	if err := doc.Parse(json); err != nil {
		t.Fatalf("Parse failed: %v", err)
	}

	root := doc.Root()
	if root.Get("name").AsString() != "Alice" {
		t.Errorf("Expected name Alice, got %s", root.Get("name").AsString())
	}
	if root.Get("age").AsInt() != 30 {
		t.Errorf("Expected age 30, got %d", root.Get("age").AsInt())
	}
	if root.Get("scores").Size() != 3 {
		t.Errorf("Expected 3 scores, got %d", root.Get("scores").Size())
	}
}

func TestUnmarshal(t *testing.T) {
	json := `{"name": "Alice", "age": 30}`
	type User struct {
		Name string `json:"name"`
		Age  int    `json:"age"`
	}
	var u User
	err := Unmarshal([]byte(json), &u)
	if err != nil {
		t.Fatalf("Unmarshal failed: %v", err)
	}
	if u.Name != "Alice" || u.Age != 30 {
		t.Errorf("Unexpected user: %+v", u)
	}
}

func TestUnmarshalMap(t *testing.T) {
	json := `{"a": 1, "b": 2.5, "c": "hello"}`
	var m map[string]interface{}
	err := Unmarshal([]byte(json), &m)
	if err != nil {
		t.Fatalf("Unmarshal failed: %v", err)
	}
	if m["a"].(int64) != 1 || m["b"].(float64) != 2.5 || m["c"].(string) != "hello" {
		t.Errorf("Unexpected map content: %v", m)
	}
}

func TestInterface(t *testing.T) {
	json := `{"a": [1, 2], "b": {"c": 3}}`
	doc := NewDocument()
	doc.Parse(json)
	val := doc.Root().ToInterface()
	m := val.(map[string]interface{})
	if len(m["a"].([]interface{})) != 2 {
		t.Errorf("Expected array size 2, got %v", m["a"])
	}
}

func TestMarshalComplex(t *testing.T) {
	type User struct {
		Name  string `json:"name"`
		Admin bool   `json:"is_admin"`
	}
	u := User{Name: "Bob", Admin: true}
	json, err := Marshal(u)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}
	// Object fields are insert order or implementation dependent
	if !strings.Contains(string(json), `"is_admin":true`) || !strings.Contains(string(json), `"name":"Bob"`) {
		t.Errorf("Unexpected JSON: %s", json)
	}

	m := map[string]int{"x": 1, "y": 2}
	json, err = Marshal(m)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}
	if !strings.Contains(string(json), `"x":1`) || !strings.Contains(string(json), `"y":2`) {
		t.Errorf("Missing keys in JSON: %s", json)
	}

	s := []int{10, 20, 30}
	json, err = Marshal(s)
	if err != nil {
		t.Fatalf("Marshal failed: %v", err)
	}
	if string(json) != "[10,20,30]" {
		t.Errorf("Unexpected array JSON: %s", json)
	}
}
