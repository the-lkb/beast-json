// fuzz_lazy.cpp – libFuzzer target for beast::json::lazy internals.
//
// Directly exercises the tape-based zero-copy lazy parser (parse_reuse),
// plus the full Value accessor / mutation / serializer surface.
// A single static DocumentView is reused across invocations so that the
// hot-path (tape.reset()) and cold-path (tape.reserve()) are both hit.
//
// Build / run: see fuzz_parse.cpp header comment; swap target name to fuzz_lazy.

#include <beast_json/beast_json.hpp>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

using namespace beast::json::lazy;

// One DocumentView per fuzzing process.
static DocumentView g_doc;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const std::string_view input(reinterpret_cast<const char *>(data), size);

    try {
        Value root = parse_reuse(g_doc, input);

        // Serialise
        (void)root.dump();
        (void)root.dump(2);

        // Type accessors
        (void)root.is_object();
        (void)root.is_array();
        (void)root.is_string();
        (void)root.is_number();
        (void)root.is_null();
        (void)root.is_bool();
        (void)root.is_valid();

        // Container access
        (void)root.size();
        (void)root.empty();

        // Scalar extraction (type-guarded)
        if (root.is_string())      { auto s = root.as<std::string_view>(); (void)s; }
        else if (root.is_bool())   { (void)root.as<bool>(); }
        else if (root.is_int())    { (void)root.as<int64_t>(); }
        else if (root.is_double()) { (void)root.as<double>(); }

        // Object iteration + mutation
        if (root.is_object()) {
            for (auto [k, v] : root.items()) {
                (void)k;
                (void)v.is_valid();
                (void)v.dump();
            }
            (void)root.find("__fuzz__");
            (void)root.contains("__fuzz__");
            root.insert("__z__", 1);
            (void)root.dump();
            root.erase("__z__");
        }

        // Array iteration + mutation
        if (root.is_array()) {
            for (auto elem : root.elements()) { (void)elem.dump(); }
            root.push_back(nullptr);
            (void)root.dump();
            if (root.size() > 0) root.erase(root.size() - 1);
        }

        // SafeValue chain
        auto sv = root.get("__x__")["y"]["z"];
        (void)sv.value_or(-1);

        // JSON Pointer
        (void)root.at("/0");
        (void)root.at("/a/b");

        // Pipe fallback
        int fallback = root["__missing__"] | 99;
        (void)fallback;

        // type_name
        (void)root.type_name();

    } catch (const std::runtime_error &) {}

    return 0;
}
