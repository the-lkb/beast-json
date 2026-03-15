import json
import time
import sys
import os
from pathlib import Path

# Paths
BUILD_DIR = Path("build")
PY_BINDINGS_DIR = BUILD_DIR / "bindings" / "python"
C_BINDINGS_DIR = BUILD_DIR / "bindings" / "c"

sys.path.append(str(PY_BINDINGS_DIR))
sys.path.append("bindings/python")

# Try to import both
try:
    import qbuem_json_native as bj_native
    HAS_NATIVE = True
except ImportError:
    HAS_NATIVE = False
    print("Warning: nanobind extension not found.")

try:
    import qbuem_json as bj_ctypes
    HAS_CTYPES = True
except Exception as e:
    HAS_CTYPES = False
    print(f"Warning: ctypes wrapper failed to load: {e}")

try:
    import orjson
    HAS_ORJSON = True
except ImportError:
    HAS_ORJSON = False

def bench(name, func, data, iterations=1000):
    start = time.perf_counter()
    for _ in range(iterations):
        res = func(data)
    end = time.perf_counter()
    avg_ms = (end - start) * 1000 / iterations
    print(f"{name:20}: {avg_ms:8.4f} ms/op")
    return avg_ms

# Sample Large-ish JSON (approx 10KB)
sample = {
    "users": [
        {"id": i, "name": f"User {i}", "active": i % 2 == 0, "scores": [j for j in range(10)]}
        for i in range(50)
    ],
    "meta": {"total": 50, "version": "1.0.6", "desc": "Benchmarking data"}
}
json_str = json.dumps(sample)

print(f"Benchmarking with {len(json_str)} bytes JSON, 1000 iterations\n")

print("##section bindings")
print("=== Language Bindings ===")

# 1. Standard json
avg_p = bench("Python stdlib", json.loads, json_str)
avg_s = bench("Python stdlib dump", lambda d: json.dumps(d), sample)
print(f"Python stdlib | Parse: {avg_p*1000:8.2f} us | Serialize: {avg_s*1000:8.2f} us | Alloc: 0 KB")

# 2. qbuem-json (ctypes)
if HAS_CTYPES:
    def ctypes_parse(s):
        doc = bj_ctypes.Document(s)
        return doc.root()
    avg_p = bench("qbuem-json (ctypes)", ctypes_parse, json_str)
    
    doc = bj_ctypes.Document(json_str)
    root = doc.root()
    avg_s = bench("qbuem-json (ctypes) dump", lambda r: r.dump(), root)
    print(f"qbuem-json (ctypes) | Parse: {avg_p*1000:8.2f} us | Serialize: {avg_s*1000:8.2f} us | Alloc: 0 KB")

# 3. qbuem-json (nanobind)
if HAS_NATIVE:
    def native_parse(s):
        doc = bj_native.loads(s)
        return doc
    avg_p = bench("qbuem-json (nanobind)", native_parse, json_str)
    
    doc = bj_native.loads(json_str)
    root = doc.root()
    avg_s = bench("qbuem-json (nanobind) dump", lambda r: r.dump(), root)
    print(f"qbuem-json (nanobind) | Parse: {avg_p*1000:8.2f} us | Serialize: {avg_s*1000:8.2f} us | Alloc: 0 KB")

# 4. orjson
if HAS_ORJSON:
    avg_p = bench("orjson", orjson.loads, json_str)
    avg_s = bench("orjson dump", orjson.dumps, sample)
    print(f"orjson | Parse: {avg_p*1000:8.2f} us | Serialize: {avg_s*1000:8.2f} us | Alloc: 0 KB")

print("\nAccess Latency (root['users'][25]['name']):")

# Access benchmarks
if HAS_NATIVE:
    native_doc = bj_native.loads(json_str)
    native_root = native_doc.root() # root is tied to doc
    def native_access(r): return r['users'][25]['name'].as_string()
    bench("nanobind access", native_access, native_root, iterations=10000)

if HAS_CTYPES:
    ctypes_doc = bj_ctypes.Document(json_str)
    ctypes_root = ctypes_doc.root()
    def ctypes_access(r): return r['users'][25]['name'].get()
    bench("ctypes access", ctypes_access, ctypes_root, iterations=10000)

py_dict = json.loads(json_str)
def py_access(d): return d['users'][25]['name']
bench("Python dict access", py_access, py_dict, iterations=10000)
