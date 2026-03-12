#!/usr/bin/env python3
"""
example.py — qbuem-json Python bindings usage demo

Build the shared library first:
    cmake -S ../.. -B ../../build -DQBUEM_JSON_BUILD_BINDINGS=ON
    cmake --build ../../build --target qbuem_json_c

Then run:
    python3 example.py
"""

import sys
import os

# Allow running from the bindings/python directory
sys.path.insert(0, os.path.dirname(__file__))

from qbuem_json import Document, loads, dumps

def main():
    # ── Basic parsing ─────────────────────────────────────────────────────────
    json_str = '''
    {
        "name": "Alice",
        "age": 30,
        "active": true,
        "score": 98.5,
        "address": {
            "city": "Seoul",
            "country": "KR"
        },
        "tags": ["admin", "user", "developer"]
    }
    '''

    doc = Document(json_str)
    root = doc.root()

    print("=== Basic Access ===")
    print(f"name:    {root['name']}")         # Alice
    print(f"age:     {int(root['age'])}")      # 30
    print(f"active:  {root['active'].get()}")  # True
    print(f"score:   {float(root['score'])}")  # 98.5
    print(f"city:    {root['address']['city']}")  # Seoul
    print(f"tag[0]:  {root['tags'][0]}")       # admin

    # ── Type queries ──────────────────────────────────────────────────────────
    print("\n=== Type Queries ===")
    print(f"name type:   {root['name'].type_name}")    # string
    print(f"age type:    {root['age'].type_name}")     # int
    print(f"score type:  {root['score'].type_name}")   # double
    print(f"active type: {root['active'].type_name}")  # bool
    print(f"tags type:   {root['tags'].type_name}")    # array
    print(f"address type:{root['address'].type_name}") # object

    # ── Container iteration ───────────────────────────────────────────────────
    print("\n=== Object iteration (items) ===")
    for key, val in root.items():
        print(f"  {key}: {val.type_name}")

    print("\n=== Array iteration ===")
    for tag in root["tags"]:
        print(f"  tag: {tag}")

    # ── JSON Pointer (RFC 6901) ───────────────────────────────────────────────
    print("\n=== JSON Pointer ===")
    print(root.at("/address/city"))  # Seoul
    print(root.at("/tags/2"))        # developer

    # ── Serialization ─────────────────────────────────────────────────────────
    print("\n=== Serialization ===")
    print(root.dump())
    print(root["address"].dump(indent=2))

    # ── Mutation ──────────────────────────────────────────────────────────────
    print("\n=== Mutation ===")
    root["name"].set("Bob")
    root.insert("version", 2)
    root["tags"].erase(0)  # remove "admin"
    print(root.dump(indent=2))

    # ── Convert to Python dict ────────────────────────────────────────────────
    print("\n=== Python dict conversion ===")
    obj = root.get()
    print(type(obj))        # <class 'dict'>
    print(obj["address"])   # {'city': 'Seoul', 'country': 'KR'}

    # ── loads() convenience ───────────────────────────────────────────────────
    print("\n=== loads() ===")
    data = loads('[1, 2, {"x": 3}]')
    print(data)   # [1, 2, {'x': 3}]

    # ── Strict RFC 8259 mode ──────────────────────────────────────────────────
    print("\n=== RFC 8259 strict mode ===")
    try:
        bad = Document("[1, 2,]", strict=True)
        print("ERROR: should have rejected trailing comma")
    except ValueError as e:
        print(f"Correctly rejected: {e}")

    try:
        ok = Document('{"a": 1}', strict=True)
        print(f"Valid JSON accepted: {ok.dump()}")
    except ValueError as e:
        print(f"ERROR: {e}")

    print("\nAll examples completed successfully!")


if __name__ == "__main__":
    main()
