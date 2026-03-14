package qbuem

import (
	"encoding/json"
	"testing"
)

var benchmarkData = []byte(`{
    "users": [
        {"id": 0, "name": "User 0", "active": true, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]},
        {"id": 1, "name": "User 1", "active": false, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]},
        {"id": 2, "name": "User 2", "active": true, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]},
        {"id": 3, "name": "User 3", "active": false, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]},
        {"id": 4, "name": "User 4", "active": true, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]}
    ],
    "meta": {"total": 5, "version": "1.0.6", "desc": "Benchmarking data"}
}`)

func BenchmarkStandardJSON(b *testing.B) {
	for i := 0; i < b.N; i++ {
		var m map[string]interface{}
		_ = json.Unmarshal(benchmarkData, &m)
	}
}

func BenchmarkQbuemJSON(b *testing.B) {
	doc := NewDocument()
	for i := 0; i < b.N; i++ {
		_ = doc.Parse(string(benchmarkData))
		_ = doc.Root()
	}
}

func BenchmarkStandardAccess(b *testing.B) {
	var m map[string]interface{}
	_ = json.Unmarshal(benchmarkData, &m)
	for i := 0; i < b.N; i++ {
		_ = m["users"].([]interface{})[2].(map[string]interface{})["name"]
	}
}

func BenchmarkQbuemAccess(b *testing.B) {
	doc := NewDocument()
	_ = doc.Parse(string(benchmarkData))
	root := doc.Root()
	for i := 0; i < b.N; i++ {
		_ = root.At("/users/2/name").AsString()
	}
}
