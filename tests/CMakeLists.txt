
# tests/CMakeLists.txt

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Removed fast_float dependency

# Sanitizer option: build tests with ASan + UBSan
option(BEAST_JSON_SANITIZE
    "Build tests with AddressSanitizer + UBSan (requires Clang or GCC >= 4.8)" OFF)

# GTest function (uses gtest_main)
function(add_beast_gtest name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE beast_json::beast_json gtest_main)
    target_compile_features(${name} PRIVATE cxx_std_20)

    if(BEAST_JSON_SANITIZE)
        target_compile_options(${name} PRIVATE
            -O1 -g
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -fno-sanitize-recover=all
        )
        target_link_options(${name} PRIVATE
            -fsanitize=address,undefined
        )
    else()
        target_compile_options(${name} PRIVATE -O1 -g -Wall -Wextra)
    endif()

    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 60 DISCOVERY_MODE PRE_TEST)
endfunction()

# ── Core correctness tests ─────────────────────────────────────────────────
add_beast_gtest(test_relaxed)
add_beast_gtest(test_rfc8259)
add_beast_gtest(test_unicode)
add_beast_gtest(test_strict_number)
add_beast_gtest(test_control_char)
add_beast_gtest(test_comments)
add_beast_gtest(test_trailing_commas)
add_beast_gtest(test_utf8_validation)
add_beast_gtest(test_duplicate_keys)
add_beast_gtest(test_serializer)
add_beast_gtest(test_errors)

# ── New: lazy API type coverage and round-trip fidelity ────────────────────
add_beast_gtest(test_lazy_types)
add_beast_gtest(test_lazy_roundtrip)
add_beast_gtest(test_value_accessors)
# Download benchmark data
set(BENCHMARK_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
if(NOT EXISTS ${BENCHMARK_DATA_DIR}/twitter.json)
    message(STATUS "Downloading twitter.json...")
    file(DOWNLOAD 
        https://raw.githubusercontent.com/simdjson/simdjson/master/jsonexamples/twitter.json
        ${BENCHMARK_DATA_DIR}/twitter.json
        SHOW_PROGRESS
    )
endif()

# Benchmark Executable

# Benchmark Executable

