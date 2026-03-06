// fuzz_parse.cpp – libFuzzer target for the beast high-level parse() API.
//
// Tests beast::parse(), Value accessors, iteration, and dump() with arbitrary
// byte inputs. AddressSanitizer + UBSanitizer catch memory errors and UB.
//
// Build:
//   cmake -B build-fuzz \
//         -DBEAST_JSON_BUILD_FUZZ=ON \
//         -DBEAST_JSON_BUILD_TESTS=OFF \
//         -DBEAST_JSON_BUILD_BENCHMARKS=OFF \
//         -DCMAKE_CXX_COMPILER=clang++
//   cmake --build build-fuzz --target fuzz_parse
//
// Run (indefinitely):
//   ./build-fuzz/fuzz/fuzz_parse fuzz/corpus/ -max_len=65536
//
// Reproduce a crash:
//   ./build-fuzz/fuzz/fuzz_parse <crash-file>

#include <beast_json/beast_json.hpp>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

// One Document per fuzzing process — reuse across invocations.
static beast::Document g_doc;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const std::string_view input(reinterpret_cast<const char *>(data), size);

    try {
        beast::Value root = beast::parse(g_doc, input);

        // Type accessors
        (void)root.is_object();
        (void)root.is_array();
        (void)root.is_string();
        (void)root.is_number();
        (void)root.is_null();
        (void)root.is_bool();
        (void)root.is_valid();

        // Dump (compact and pretty)
        (void)root.dump();
        (void)root.dump(2);

        // Container size
        (void)root.size();
        (void)root.empty();

        // Object iteration
        if (root.is_object()) {
            for (auto [k, v] : root.items()) {
                (void)k;
                (void)v.is_valid();
                (void)v.dump();
            }
        }

        // Array iteration
        if (root.is_array()) {
            for (auto elem : root.elements()) {
                (void)elem.dump();
            }
        }

        // SafeValue chain (never throws)
        (void)root.get("__fuzz__")["x"].value_or(0);

        // JSON Pointer (runtime path — exercises bounds checks)
        (void)root.at("/0");
        (void)root.at("/a/b");

        // Pipe fallback operator
        int fb = root["__missing__"] | 99;
        (void)fb;

    } catch (const std::exception &) {
        // Parse errors and type errors are expected.
    }

    return 0;
}
