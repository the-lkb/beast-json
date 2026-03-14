"""
qbuem_json.py — Python bindings for qbuem-json via ctypes

Usage:
    from qbuem_json import QbuemJSON, Document

    # Parse
    doc = Document('{"name":"Alice","age":30,"tags":["a","b"]}')
    root = doc.root()

    # Access
    print(root["name"])         # Alice
    print(root["age"])          # 30
    print(root["tags"][0])      # a

    # Iterate
    for key, val in root.items():
        print(f"{key}: {val}")

    # Read scalar at path
    print(root.at("/tags/1"))   # b

    # Serialize
    print(root.dump())
    print(root.dump(indent=2))

    # Mutation
    root["name"] = "Bob"
    root.insert("extra", 42)
    root["tags"].erase(0)

    # Strict RFC 8259 mode
    try:
        doc2 = Document("[1,2,]", strict=True)
    except ValueError as e:
        print(e)  # RFC 8259 violation: trailing comma

Requirements:
    The shared library 'liqbuem_json_c.so' (or .dylib/.dll) must be
    findable via LD_LIBRARY_PATH / DYLD_LIBRARY_PATH.

    Build it with:
        cmake -S . -B build -DQBUEM_JSON_BUILD_BINDINGS=ON
        cmake --build build --target qbuem_json_c
"""

import ctypes
import ctypes.util
import os
import platform
import sys
from pathlib import Path
from typing import Any, Dict, Iterator, List, Optional, Tuple, Union

# ── Library loading ───────────────────────────────────────────────────────────

def _find_library() -> ctypes.CDLL:
    """Search for liqbuem_json_c in common locations."""
    system = platform.system()
    if system == "Linux":
        names = ["libqbuem_json_c.so", "libqbuem_json_c.so.1",
                 "libqbuem_json_c.so.1.0.0"]
    elif system == "Darwin":
        names = ["libqbuem_json_c.dylib", "libqbuem_json_c.1.dylib"]
    elif system == "Windows":
        names = ["qbuem_json_c.dll"]
    else:
        names = ["libqbuem_json_c.so"]

    # Search paths: same dir as this script, LD_LIBRARY_PATH, then system
    search_dirs = [
        str(Path(__file__).parent),
        str(Path(__file__).parent.parent.parent / "build" / "bindings" / "c"),
        str(Path(__file__).parent.parent.parent / "build"),
    ]
    env_path = os.environ.get("QBUEM_JSON_LIB_PATH", "")
    if env_path:
        search_dirs.insert(0, env_path)

    for d in search_dirs:
        for name in names:
            candidate = os.path.join(d, name)
            if os.path.exists(candidate):
                return ctypes.CDLL(candidate)

    # Fall back to system linker
    found = ctypes.util.find_library("qbuem_json_c")
    if found:
        return ctypes.CDLL(found)

    raise RuntimeError(
        "Could not find liqbuem_json_c. "
        "Build it with: cmake --build <build-dir> --target qbuem_json_c\n"
        "Then set QBUEM_JSON_LIB_PATH=/path/to/dir"
    )


_lib: Optional[ctypes.CDLL] = None

def _get_lib() -> ctypes.CDLL:
    global _lib
    if _lib is None:
        _lib = _find_library()
        _setup_signatures(_lib)
    return _lib


def _setup_signatures(lib: ctypes.CDLL) -> None:
    """Configure argtypes/restype for all C API functions."""
    c_str   = ctypes.c_char_p
    c_szt   = ctypes.c_size_t
    c_int   = ctypes.c_int
    c_int64 = ctypes.c_int64
    c_dbl   = ctypes.c_double
    c_void  = ctypes.c_void_p  # opaque pointers
    c_uint32 = ctypes.c_uint32

    class QbuemJSONValue(ctypes.Structure):
        _fields_ = [("state", c_void),
                    ("index", c_uint32)]

    lib.QbuemJSONValue = QbuemJSONValue

    lib.qbuem_json_doc_create.restype  = c_void
    lib.qbuem_json_doc_create.argtypes = []

    lib.qbuem_json_doc_destroy.restype  = None
    lib.qbuem_json_doc_destroy.argtypes = [c_void]

    lib.qbuem_json_parse.restype  = QbuemJSONValue
    lib.qbuem_json_parse.argtypes = [c_void, c_str, c_szt]

    lib.qbuem_json_parse_strict.restype  = QbuemJSONValue
    lib.qbuem_json_parse_strict.argtypes = [c_void, c_str, c_szt]

    lib.qbuem_json_last_error.restype  = c_str
    lib.qbuem_json_last_error.argtypes = [c_void]

    lib.qbuem_json_type.restype  = c_int
    lib.qbuem_json_type.argtypes = [QbuemJSONValue]

    lib.qbuem_json_type_name.restype  = c_str
    lib.qbuem_json_type_name.argtypes = [QbuemJSONValue]

    lib.qbuem_json_is_valid.restype  = c_int
    lib.qbuem_json_is_valid.argtypes = [QbuemJSONValue]

    lib.qbuem_json_as_bool.restype  = c_int
    lib.qbuem_json_as_bool.argtypes = [QbuemJSONValue]

    lib.qbuem_json_as_int.restype  = c_int64
    lib.qbuem_json_as_int.argtypes = [QbuemJSONValue]

    lib.qbuem_json_as_double.restype  = c_dbl
    lib.qbuem_json_as_double.argtypes = [QbuemJSONValue]

    lib.qbuem_json_as_string.restype  = c_str
    lib.qbuem_json_as_string.argtypes = [QbuemJSONValue, ctypes.POINTER(c_szt)]

    lib.qbuem_json_size.restype  = c_szt
    lib.qbuem_json_size.argtypes = [QbuemJSONValue]

    lib.qbuem_json_empty.restype  = c_int
    lib.qbuem_json_empty.argtypes = [QbuemJSONValue]

    lib.qbuem_json_get_idx.restype  = QbuemJSONValue
    lib.qbuem_json_get_idx.argtypes = [QbuemJSONValue, c_szt]

    lib.qbuem_json_get_key.restype  = QbuemJSONValue
    lib.qbuem_json_get_key.argtypes = [QbuemJSONValue, c_str]

    lib.qbuem_json_at_path.restype  = QbuemJSONValue
    lib.qbuem_json_at_path.argtypes = [QbuemJSONValue, c_str]

    lib.qbuem_json_iter_create.restype  = c_void
    lib.qbuem_json_iter_create.argtypes = [QbuemJSONValue]

    lib.qbuem_json_iter_next.restype  = c_int
    lib.qbuem_json_iter_next.argtypes = [c_void]

    lib.qbuem_json_iter_key.restype  = c_str
    lib.qbuem_json_iter_key.argtypes = [c_void, ctypes.POINTER(c_szt)]

    lib.qbuem_json_iter_value.restype  = QbuemJSONValue
    lib.qbuem_json_iter_value.argtypes = [c_void]

    lib.qbuem_json_iter_destroy.restype  = None
    lib.qbuem_json_iter_destroy.argtypes = [c_void]

    lib.qbuem_json_dump.restype  = c_str
    lib.qbuem_json_dump.argtypes = [c_void, QbuemJSONValue, ctypes.POINTER(c_szt)]

    lib.qbuem_json_dump_pretty.restype  = c_str
    lib.qbuem_json_dump_pretty.argtypes = [c_void, QbuemJSONValue, c_int,
                                       ctypes.POINTER(c_szt)]

    lib.qbuem_json_set_int.restype  = None
    lib.qbuem_json_set_int.argtypes = [QbuemJSONValue, c_int64]

    lib.qbuem_json_set_double.restype  = None
    lib.qbuem_json_set_double.argtypes = [QbuemJSONValue, c_dbl]

    lib.qbuem_json_set_string.restype  = None
    lib.qbuem_json_set_string.argtypes = [QbuemJSONValue, c_str, c_szt]

    lib.qbuem_json_set_null.restype  = None
    lib.qbuem_json_set_null.argtypes = [QbuemJSONValue]

    lib.qbuem_json_set_bool.restype  = None
    lib.qbuem_json_set_bool.argtypes = [QbuemJSONValue, c_int]

    lib.qbuem_json_insert_raw.restype  = None
    lib.qbuem_json_insert_raw.argtypes = [QbuemJSONValue, c_str, c_str]

    lib.qbuem_json_erase_key.restype  = None
    lib.qbuem_json_erase_key.argtypes = [QbuemJSONValue, c_str]

    lib.qbuem_json_erase_idx.restype  = None
    lib.qbuem_json_erase_idx.argtypes = [QbuemJSONValue, c_szt]


# ── QbuemJSONType enum ────────────────────────────────────────────────────────────

QbuemJSON_TYPE_INVALID = 0
QbuemJSON_TYPE_NULL    = 1
QbuemJSON_TYPE_BOOL    = 2
QbuemJSON_TYPE_INT     = 3
QbuemJSON_TYPE_DOUBLE  = 4
QbuemJSON_TYPE_STRING  = 5
QbuemJSON_TYPE_ARRAY   = 6
QbuemJSON_TYPE_OBJECT  = 7

# ── Value wrapper ─────────────────────────────────────────────────────────────

class Value:
    """
    Wraps a QbuemJSONValue* — zero-copy lazy accessor.

    Lifetime: tied to the parent Document.  Do not use a Value after its
    Document has been garbage-collected.
    """

    __slots__ = ("_val_struct", "_lib", "_doc")
 
    def __init__(self, val_struct, lib, doc=None):
        self._val_struct = val_struct
        self._lib = lib
        self._doc = doc

    # ── Type queries ──────────────────────────────────────────────────────────

    @property
    def type(self) -> int:
        return self._lib.qbuem_json_type(self._val_struct)

    @property
    def type_name(self) -> str:
        b = self._lib.qbuem_json_type_name(self._val_struct)
        return b.decode() if b else "invalid"

    def is_valid(self) -> bool:
        return bool(self._lib.qbuem_json_is_valid(self._val_struct))

    def is_null(self)   -> bool: return self.type == QbuemJSON_TYPE_NULL
    def is_bool(self)   -> bool: return self.type == QbuemJSON_TYPE_BOOL
    def is_int(self)    -> bool: return self.type == QbuemJSON_TYPE_INT
    def is_double(self) -> bool: return self.type == QbuemJSON_TYPE_DOUBLE
    def is_number(self) -> bool: return self.type in (QbuemJSON_TYPE_INT, QbuemJSON_TYPE_DOUBLE)
    def is_string(self) -> bool: return self.type == QbuemJSON_TYPE_STRING
    def is_array(self)  -> bool: return self.type == QbuemJSON_TYPE_ARRAY
    def is_object(self) -> bool: return self.type == QbuemJSON_TYPE_OBJECT

    def __bool__(self) -> bool:
        return self.is_valid()

    # ── Conversion to Python ──────────────────────────────────────────────────

    def get(self) -> Any:
        """Convert to the most natural Python type."""
        t = self.type
        if t == QbuemJSON_TYPE_NULL:    return None
        if t == QbuemJSON_TYPE_BOOL:    return bool(self._lib.qbuem_json_as_bool(self._val_struct))
        if t == QbuemJSON_TYPE_INT:     return int(self._lib.qbuem_json_as_int(self._val_struct))
        if t == QbuemJSON_TYPE_DOUBLE:  return float(self._lib.qbuem_json_as_double(self._val_struct))
        if t == QbuemJSON_TYPE_STRING:
            ln = ctypes.c_size_t(0)
            p  = self._lib.qbuem_json_as_string(self._val_struct, ctypes.byref(ln))
            return p[:ln.value].decode("utf-8", errors="replace") if p else ""
        if t == QbuemJSON_TYPE_ARRAY:
            return [self[i].get() for i in range(len(self))]
        if t == QbuemJSON_TYPE_OBJECT:
            return {k: v.get() for k, v in self.items()}
        return None

    def __int__(self)   -> int:   return int(self._lib.qbuem_json_as_int(self._val_struct))
    def __float__(self) -> float: return float(self._lib.qbuem_json_as_double(self._val_struct))
    def __str__(self)   -> str:   return self.get() if self.is_string() else self.dump()

    def __repr__(self) -> str:
        return f"Value({self.dump()!r})"

    # ── Container access ──────────────────────────────────────────────────────

    def __len__(self) -> int:
        return int(self._lib.qbuem_json_size(self._val_struct))
 
    def __getitem__(self, key: Union[str, int]) -> "Value":
        if isinstance(key, int):
            vs = self._lib.qbuem_json_get_idx(self._val_struct,
                                          ctypes.c_size_t(key))
        else:
            vs = self._lib.qbuem_json_get_key(self._val_struct,
                                          key.encode())
        return Value(vs, self._lib, self._doc)

    def at(self, path: str) -> "Value":
        """RFC 6901 JSON Pointer access: value.at('/a/b/0')"""
        vs = self._lib.qbuem_json_at_path(self._val_struct,
                                      path.encode())
        return Value(vs, self._lib, self._doc)

    def items(self) -> Iterator[Tuple[str, "Value"]]:
        """Iterate over object key-value pairs."""
        it = self._lib.qbuem_json_iter_create(self._val_struct)
        if not it:
            return
        try:
            while self._lib.qbuem_json_iter_next(it):
                ln  = ctypes.c_size_t(0)
                kb  = self._lib.qbuem_json_iter_key(it, ctypes.byref(ln))
                key = kb[:ln.value].decode("utf-8", errors="replace") if kb else ""
                vs  = self._lib.qbuem_json_iter_value(it)
                yield key, Value(vs, self._lib, self._doc)
        finally:
            self._lib.qbuem_json_iter_destroy(it)

    def keys(self) -> Iterator[str]:
        for k, _ in self.items():
            yield k

    def values(self) -> Iterator["Value"]:
        for _, v in self.items():
            yield v

    def __iter__(self):
        """Iterate array elements or object keys."""
        if self.is_array():
            for i in range(len(self)):
                yield self[i]
        elif self.is_object():
            yield from self.keys()

    def __contains__(self, key: str) -> bool:
        return self[key].is_valid()

    # ── Serialization ─────────────────────────────────────────────────────────

    def dump(self, indent: int = 0) -> str:
        ln = ctypes.c_size_t(0)
        doc_ptr = self._doc._doc_ptr if self._doc else None
        if not doc_ptr:
            # Fallback for loads() where we don't hold the doc explicitly
            return ""
        if indent > 0:
            b = self._lib.qbuem_json_dump_pretty(doc_ptr,
                                             self._val_struct,
                                             ctypes.c_int(indent),
                                             ctypes.byref(ln))
        else:
            b = self._lib.qbuem_json_dump(doc_ptr,
                                      self._val_struct,
                                      ctypes.byref(ln))
        return b[:ln.value].decode("utf-8", errors="replace") if b else ""

    # ── Mutation ──────────────────────────────────────────────────────────────

    def __setitem__(self, key: Union[str, int], value: Any) -> None:
        """Mutate a field.  value can be int/float/str/bool/None."""
        child = self[key]
        _set_value(self._lib, child._val_struct, value)

    def insert(self, key: str, value: Any) -> None:
        """Insert a new key-value pair into this object."""
        raw = _to_raw_json(value)
        self._lib.qbuem_json_insert_raw(self._val_struct, key.encode(), raw.encode())

    def erase(self, key: Union[str, int]) -> None:
        if isinstance(key, int):
            self._lib.qbuem_json_erase_idx(self._val_struct, ctypes.c_size_t(key))
        else:
            self._lib.qbuem_json_erase_key(self._val_struct, key.encode())

    def set(self, value: Any) -> None:
        """Set this scalar value."""
        _set_value(self._lib, self._val_struct, value)


def _set_value(lib, val_ptr, value: Any) -> None:
    if value is None:
        lib.qbuem_json_set_null(val_ptr)
    elif isinstance(value, bool):
        lib.qbuem_json_set_bool(val_ptr, ctypes.c_int(1 if value else 0))
    elif isinstance(value, int):
        lib.qbuem_json_set_int(val_ptr, ctypes.c_int64(value))
    elif isinstance(value, float):
        lib.qbuem_json_set_double(val_ptr, ctypes.c_double(value))
    elif isinstance(value, str):
        b = value.encode("utf-8")
        lib.qbuem_json_set_string(val_ptr, b, ctypes.c_size_t(len(b)))
    else:
        raise TypeError(f"Cannot set value of type {type(value).__name__}")


def _to_raw_json(value: Any) -> str:
    """Convert a Python value to a raw JSON string for insertion."""
    import json
    return json.dumps(value, ensure_ascii=False)


# ── Document wrapper ──────────────────────────────────────────────────────────

class Document:
    """
    qbuem-json document context.

    Parses JSON and provides access to the root Value.

    Example::

        doc = Document('{"a":1,"b":[2,3]}')
        root = doc.root()
        print(root["a"])       # 1
        print(root["b"][1])    # 3
        print(root.dump())

    Args:
        json_str: JSON string or bytes to parse.
        strict:   If True, enforces RFC 8259 (rejects trailing commas, etc.).
    """

    def __init__(self, json_str: Union[str, bytes], strict: bool = False):
        lib = _get_lib()
        self._lib = lib
        self._doc_ptr = lib.qbuem_json_doc_create()
        if not self._doc_ptr:
            raise MemoryError("qbuem_json_doc_create() returned NULL")

        # Keep a reference to the encoded bytes so the string stays alive
        if isinstance(json_str, str):
            json_bytes = json_str.encode("utf-8")
        else:
            json_bytes = json_str
        self._json_bytes = json_bytes  # keep alive

        if strict:
            vs = lib.qbuem_json_parse_strict(self._doc_ptr, json_bytes,
                                         len(json_bytes))
        else:
            vs = lib.qbuem_json_parse(self._doc_ptr, json_bytes, len(json_bytes))

        if not vs.state:
            err = lib.qbuem_json_last_error(self._doc_ptr)
            msg = err.decode() if err else "parse failed"
            lib.qbuem_json_doc_destroy(self._doc_ptr)
            self._doc_ptr = None
            raise ValueError(f"JSON parse error: {msg}")

        self._root_struct = vs
        self._root_value = Value(vs, self._lib, self)

    def __del__(self):
        if self._doc_ptr:
            self._lib.qbuem_json_doc_destroy(self._doc_ptr)
            self._doc_ptr = None

    def root(self) -> Value:
        """Return the root Value."""
        return self._root_value

    # Convenience: proxy common Value methods directly on the Document
    def __getitem__(self, key):
        return self.root()[key]

    def __len__(self):
        return len(self.root())

    def dump(self, indent: int = 0) -> str:
        return self.root().dump(indent)

    def items(self):
        return self.root().items()

    def keys(self):
        return self.root().keys()


# ── Convenience functions ─────────────────────────────────────────────────────

def loads(json_str: Union[str, bytes], strict: bool = False) -> Any:
    """
    Parse a JSON string and return the equivalent Python object.

    Like json.loads() but uses qbuem-json for parsing.

    Returns: dict, list, str, int, float, bool, or None.
    """
    doc = Document(json_str, strict=strict)
    return doc.root().get()


def dumps(obj: Any) -> str:
    """
    Serialize a Python object to a JSON string via qbuem-json.

    For simple types (dict, list, str, int, float, bool, None),
    this is equivalent to json.dumps().
    """
    import json as _json
    return _json.dumps(obj, ensure_ascii=False)
