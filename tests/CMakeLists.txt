
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
option(BEAST_JSON_SANITIZE_THREAD
    "Build tests with ThreadSanitizer (TSan) to detect data races" OFF)
option(BEAST_JSON_SANITIZE_MEMORY
    "Build tests with MemorySanitizer (MSan) to detect uninitialized reads (Clang/Linux only)" OFF)

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
    elseif(BEAST_JSON_SANITIZE_THREAD)
        target_compile_options(${name} PRIVATE
            -O1 -g -fsanitize=thread -fno-omit-frame-pointer
        )
        target_link_options(${name} PRIVATE -fsanitize=thread)
    elseif(BEAST_JSON_SANITIZE_MEMORY)
        target_compile_options(${name} PRIVATE
            -O1 -g -fsanitize=memory -fsanitize-memory-track-origins -fPIE -fno-omit-frame-pointer
        )
        target_link_options(${name} PRIVATE -fsanitize=memory -pie)
    else()
        target_compile_options(${name} PRIVATE -O1 -g -Wall -Wextra)
    endif()

    # POST_BUILD discovers tests once at build time by running the binary on the
    # build host, which is deterministic and avoids running each binary twice
    # (once for discovery, once for execution) under PRE_TEST mode.
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 90 DISCOVERY_MODE POST_BUILD)
endfunction()

# ── Core correctness tests ─────────────────────────────────────────────────
add_beast_gtest(test_relaxed)
add_beast_gtest(test_compliance)
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
add_beast_gtest(test_mutations)
add_beast_gtest(repro_bugs)
add_beast_gtest(test_stl_exhaustive)
add_beast_gtest(test_complex_stl)
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

