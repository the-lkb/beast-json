# HFT Optimization Patterns

In High-Frequency Trading (HFT) and ultra-low latency systems, every microsecond of jitter matters. Beast JSON provides specialized patterns to minimize latency and memory stalls.

## 🧵 Zero-Jitter Allocations

Beast JSON's `TapeDOM` is designed to be reuse-friendly. In a hot loop, you should avoid re-allocating the Tape memory.

### Pattern: Document Reuse
Instead of creating a new document object for every JSON packet, reuse the same workspace:

```cpp
beast::Document doc; // Pre-allocate on startup
doc.reserve(1024 * 64);   // Warm up with 64KB

while (running) {
    auto message = socket.receive();
    beast::Value root = beast::parse_reuse(doc, message);
    if (!root.is_valid()) continue;
    // Process root["field"]...
}
```
`beast::parse_reuse()` resets the tape head to the beginning without deallocating the underlying vector, effectively making the parser's allocation cost **zero**.

## 🧠 Custom Memory Resources (std::pmr)

For even tighter control, use `std::pmr::memory_resource` to point Beast JSON to a stack-based buffer or a fixed-size pool.

```cpp
std::byte buffer[1024 * 16];
std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));

// Parse using the stack pool
auto doc = beast::json::parse(json, &pool);
```
This ensures that parsing **never** triggers a heap allocation, providing deterministic timing suitable for hard real-time constraints.

## 🚄 Prefetching & Cold-Path Hints

Beast JSON uses `BEAST_LIKELY` and `BEAST_UNLIKELY` macros to guide the compiler's branch predictor. 

When navigating the DOM:
- **Object Key Access**: If you know a key is present 99% of the time, the parser is optimized to skip mutation checks.
- **Error Paths**: Throws and error-checks are placed in "cold" sections to keep the instruction cache clean for the "hot" data path.

## 🏁 Static-String Optimization

If you are serializing known schemas, use the `BEAST_JSON_FIELDS` macro. It generates code that serializes keys as static string constants, allowing the compiler to optimize the output buffer writes into large SIMD stores.
