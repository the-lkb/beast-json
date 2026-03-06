# fuzz/CMakeLists.txt – libFuzzer targets for beast-json
#
# Prerequisites
# ─────────────
#   • Clang (any version with libFuzzer support; tested with 18)
#   • libclang-rt-dev  (Ubuntu: `sudo apt install libclang-rt-18-dev`)
#
# Quick-start
# ───────────
#   cmake -B build-fuzz \
#         -DBEAST_JSON_BUILD_FUZZ=ON \
#         -DBEAST_JSON_BUILD_TESTS=OFF \
#         -DBEAST_JSON_BUILD_BENCHMARKS=OFF \
#         -DCMAKE_CXX_COMPILER=clang++
#   cmake --build build-fuzz
#
#   # Run indefinitely (Ctrl-C to stop; crashes saved as crash-*)
#   ./build-fuzz/fuzz/fuzz_parse    fuzz/corpus/ -max_len=65536
#   ./build-fuzz/fuzz/fuzz_lazy     fuzz/corpus/ -max_len=65536
#   ./build-fuzz/fuzz/fuzz_rfc8259  fuzz/corpus/ -max_len=65536
#
#   # Reproduce a specific crash
#   ./build-fuzz/fuzz/fuzz_parse  <crash-file>

# ── Compiler guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
        "BEAST_JSON_BUILD_FUZZ requires Clang (pass -DCMAKE_CXX_COMPILER=clang++). "
        "Detected compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# ── Sanitizer flags ───────────────────────────────────────────────────────────
set(FUZZ_SANITIZERS "fuzzer,address,undefined")

# ── Helper function ───────────────────────────────────────────────────────────
function(add_fuzz_target name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE beast_json::beast_json)
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_compile_options(${name} PRIVATE
        -g
        -O1
        -fsanitize=${FUZZ_SANITIZERS}
        -fno-omit-frame-pointer   # Better stack traces in crash reports
    )
    target_link_options(${name} PRIVATE
        -fsanitize=${FUZZ_SANITIZERS}
    )
endfunction()

# ── Fuzz targets ──────────────────────────────────────────────────────────────
add_fuzz_target(fuzz_parse)    # High-level beast::parse() + full accessor surface
add_fuzz_target(fuzz_lazy)     # Lazy tape parser: beast::json::lazy::parse_reuse()
add_fuzz_target(fuzz_rfc8259)  # parse_reuse consistency: tape-reuse vs fresh allocation
