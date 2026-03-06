// fuzz_rfc8259.cpp – libFuzzer target exercising the two parse paths.
//
// Parses each input twice through different DocumentView instances to
// verify parse_reuse consistency: the same valid input must produce the
// same dump() output regardless of whether the tape was freshly allocated
// or recycled.
//
// ASan detects use-after-free / OOB; UBSan detects signed overflow, etc.

#include <beast_json/beast_json.hpp>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

using namespace beast::json::lazy;

static DocumentView g_doc_a;
static DocumentView g_doc_b;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const std::string_view input(reinterpret_cast<const char *>(data), size);

    // Parse through doc_a (may grow tape)
    std::string dump_a;
    try {
        Value root = parse_reuse(g_doc_a, input);
        dump_a = root.dump();
        (void)root.dump(2);
    } catch (const std::runtime_error &) {
        return 0;  // Invalid JSON — both paths would fail.
    }

    // Parse through doc_b (independent tape — tests tape reset vs realloc)
    try {
        Value root = parse_reuse(g_doc_b, input);
        std::string dump_b = root.dump();

        // Both docs must produce identical output for the same valid input.
        if (dump_a != dump_b) __builtin_trap();

    } catch (const std::runtime_error &) {
        // doc_a succeeded but doc_b failed → bug
        __builtin_trap();
    }

    return 0;
}
