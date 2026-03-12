# Custom Allocators

qbuem-json is designed for environments where memory management is critical. It fully supports the C++17/20 **Polymorphic Memory Resource (PMR)** model, allowing you to control exactly where and how memory is allocated.

## 🧠 Why Custom Allocators?

While the default allocator is fast, certain scenarios benefit from specialized memory management:
1. **Low Latency (HFT)**: Avoid system-wide locks in `malloc` by using thread-local pool allocators.
2. **Embedded Systems**: Use a fixed-size stack buffer to ensure the parser never touches the heap.
3. **Security**: Use "Scrubbing" allocators that zero-out memory after use.

## 🛠️ Using `std::pmr`

The `qbuem::json::Document` class and the `parse` functions accept an optional `std::pmr::memory_resource*`.

### 1. Stack-Based Parsing (Zero Heap)
Use a `std::pmr::monotonic_buffer_resource` with a stack-allocated array for absolute peak performance and zero heap noise.

```cpp
#include <memory_resource>

void process_small_json(std::string_view json) {
    // 16KB stack buffer
    std::byte buffer[1024 * 16];
    std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));

    // This document and its Tape will reside entirely on the stack
    auto doc = qbuem::json::parse(json, &pool);
    
    // ... use doc ...
}
```

### 2. Global Pool Allocators
For high-concurrency servers, use a pool allocator to reduce contention.

```cpp
std::pmr::synchronized_pool_resource global_pool;

void on_request(std::string_view payload) {
    auto doc = qbuem::json::parse(payload, &global_pool);
    // ...
}
```

## 🏗️ Technical Details

qbuem-json's internal `Tape` is a `std::pmr::vector<TapeNode>`. 
- When a `memory_resource` is provided, the vector uses it for all growth operations.
- All subsequent `Value` accessors and `dump()` operations inherit this memory context where appropriate.

## 🚀 Performance Impact

Using a `monotonic_buffer_resource` for small-to-medium JSON documents typically results in a **15-20% speedup** in parsing time because it eliminates the overhead of the system's global heap manager.

---

> [!TIP]
> Combine custom allocators with [Document Reuse](/guide/low-latency-patterns#the-fundamental-pattern-document-reuse) for the ultimate steady-state optimization where memory is allocated once on startup and never touched again.
