cmake_minimum_required(VERSION 3.14)
project(beast_json_benchmarks)

include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# Fetch RapidJSON
set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG master
)
FetchContent_MakeAvailable(rapidjson)

# Fetch simdjson
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.10.1
)
FetchContent_MakeAvailable(simdjson)

# Fetch yyjson
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.10.0
)
set(YYJSON_DISABLE_SIMD OFF CACHE BOOL "Disable SIMD in yyjson" FORCE)
FetchContent_MakeAvailable(yyjson)
# Apply -march=native to yyjson on x86_64 for fair comparison (enables AVX2/AVX-512 in yyjson)
if(TARGET yyjson AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    target_compile_options(yyjson PRIVATE -march=native)
endif()

# Fetch fast_float removed

# Check for C++23 support
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++23" HAS_CXX23)
check_cxx_compiler_flag("-std=c++2b" HAS_CXX2B)
if(HAS_CXX23 OR HAS_CXX2B)
    set(BEAST_BUILD_GLAZE ON)
else()
    set(BEAST_BUILD_GLAZE OFF)
    message(STATUS "Compiler does not support C++23. Skipping Glaze benchmarks.")
endif()

if(BEAST_BUILD_GLAZE)
    # Fetch Glaze (C++23)
    FetchContent_Declare(
        glaze
        GIT_REPOSITORY https://github.com/stephenberry/glaze.git
        GIT_TAG v2.9.5
    )
    FetchContent_MakeAvailable(glaze)
endif()

# Download benchmark data
set(BENCHMARK_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(BENCHMARK_FILES
    "twitter.json"
    "canada.json"
    "citm_catalog.json"
    "gsoc-2018.json"
)

foreach(file ${BENCHMARK_FILES})
    if(NOT EXISTS ${BENCHMARK_DATA_DIR}/${file})
        message(STATUS "Downloading ${file}...")
        file(DOWNLOAD 
            https://raw.githubusercontent.com/simdjson/simdjson-data/master/jsonexamples/${file}
            ${BENCHMARK_DATA_DIR}/${file}
            SHOW_PROGRESS
        )
    endif()
endforeach()

# Optimization and PGO
# Note: -fuse-ld=lld is incompatible with GCC LTO (GIMPLE IR).
# Use the default linker (bfd/gold) which understands GCC LTO bitcode.
option(BEAST_DISABLE_LTO "Disable LTO for low-memory environments" OFF)

if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-O3)
    if(NOT BEAST_DISABLE_LTO)
        add_compile_options(-flto)
        add_link_options(-flto)
    endif()
endif()

# Phase 47: Profile-Guided Optimization (PGO)
#
# Clang-native flow (used when compiler is Clang):
#   Step 1 — instrument:
#     cmake -DPGO_MODE=GENERATE -B build_bench && cd build_bench && ninja bench_quick bench_all
#     cd benchmarks && ./bench_quick --iter 30 --all && ./bench_all --iter 30 --all
#     llvm-profdata merge *.profraw -o pgo.profdata
#   Step 2 — optimized:
#     cmake -DPGO_MODE=USE -DPGO_PROFILE_FILE=<abs-path>/pgo.profdata -B build_bench
#     ninja bench_quick bench_all
#
# GCC flow (non-Clang):
#   Step 1: cmake -DPGO_MODE=GENERATE && ninja && run_benchmarks
#   Step 2: cmake -DPGO_MODE=USE && ninja
set(PGO_MODE "" CACHE STRING "PGO mode: GENERATE or USE (leave empty for normal build)")
set(PGO_PROFILE_FILE "" CACHE FILEPATH "Path to .profdata file for Clang PGO USE mode")

if(PGO_MODE STREQUAL "GENERATE")
    message(STATUS "PGO: instrument build (GENERATE)")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-fprofile-instr-generate)
        add_link_options(-fprofile-instr-generate)
    else()
        add_compile_options(-fprofile-generate)
        add_link_options(-fprofile-generate)
    endif()
elseif(PGO_MODE STREQUAL "USE")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(NOT PGO_PROFILE_FILE)
            message(FATAL_ERROR "PGO USE with Clang requires -DPGO_PROFILE_FILE=<path/to/pgo.profdata>")
        endif()
        message(STATUS "PGO: Clang optimized build — ${PGO_PROFILE_FILE}")
        add_compile_options(-fprofile-instr-use=${PGO_PROFILE_FILE})
        add_link_options(-fprofile-instr-use=${PGO_PROFILE_FILE})
    else()
        message(STATUS "PGO: GCC optimized build — reading .gcda profiles from build tree")
        add_compile_options(-fprofile-use -fprofile-correction)
        add_link_options(-fprofile-use -fprofile-correction)
    endif()
endif()



# Unified benchmark: beast::lazy + yyjson + nlohmann, all 4 standard files
# Usage: ./bench_all [file.json]  OR  ./bench_all --all
add_executable(bench_all bench_all.cpp)
target_link_libraries(bench_all PRIVATE
    beast_json::beast_json
    nlohmann_json::nlohmann_json
    yyjson
    simdjson
)
target_include_directories(bench_all PRIVATE ${rapidjson_SOURCE_DIR}/include)

if(BEAST_BUILD_GLAZE)
    target_link_libraries(bench_all PRIVATE glaze::glaze)
    target_compile_definitions(bench_all PRIVATE BEAST_HAS_GLAZE=1)
    # Attempt to use cxx_std_23 if CMake knows it, else fallback to standard options
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.20" OR "cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        target_compile_features(bench_all PRIVATE cxx_std_23)
    elseif(HAS_CXX23)
        target_compile_options(bench_all PRIVATE -std=c++23)
    else()
        target_compile_options(bench_all PRIVATE -std=c++2b)
    endif()
else()
    target_compile_features(bench_all PRIVATE cxx_std_20)
endif()



# ── Architecture-specific flags ───────────────────────────────────────────────
# The AArch64 space is NOT monolithic. Three distinct sub-targets require
# different flag strategies:
#
#   (A) x86_64 (Intel/AMD):
#       -march=native → enables AVX-512/AVX2/SSE4.2 depending on CPU.
#       Auto-vectorization safe; LTO safe.
#
#   (B) Apple Silicon (macOS AArch64 — M1/M2/M3/M4):
#       -march=native → enables NEON + DOTPROD + SHA3 (M2+) + crypto.
#       SVE does NOT exist on Apple Silicon → zero SIGILL risk.
#       -fno-vectorize is WRONG here: it prevents compiler from using DOTPROD
#         and other extensions exposed by -march=native.
#       LTO is safe on Apple (uses lld/ld64, no GIMPLE LTO issue).
#       yyjson must also get -march=native for fair comparison (yyjson has its
#         own NEON paths gated on __ARM_NEON + compiler DOTPROD hints).
#
#   (C) Android/Linux AArch64 (Snapdragon, Graviton, etc.) + Clang:
#       SVE hardware may be present but Android kernels < 5.16 do NOT expose
#         SVE to userspace → any SVE instruction triggers SIGILL at runtime.
#       Clang with -march=native + LTO emits SVE at link time even when source
#         only uses arm_neon.h intrinsics (LTO can inline + auto-vectorize).
#       -fno-vectorize -fno-slp-vectorize -fno-lto: required safety guards.
#       beast_json's explicit NEON intrinsics are unaffected by -fno-vectorize.
#       Graviton 3/4 (Linux, SVE exposed): use -march=armv8.4-a+sve explicitly
#         rather than -march=native to avoid accidental SVE2/SME emission.

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    # (A) x86_64: native ISA; LTO and auto-vectorization intact.
    foreach(_tgt bench_all)
        if(TARGET ${_tgt})
            target_compile_options(${_tgt} PRIVATE -march=native)
        endif()
    endforeach()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # (B) Apple Silicon — no SVE, safe to use -march=native + auto-vectorize.
        # This unlocks DOTPROD (UDOT/SDOT), SHA3/EOR3 (M2+), and correct
        # BEAST_PREFETCH_DISTANCE (512B) via BEAST_ARCH_APPLE_SILICON macro.
        foreach(_tgt bench_all)
            if(TARGET ${_tgt})
                target_compile_options(${_tgt} PRIVATE -march=native)
            endif()
        endforeach()
        # Give yyjson the same ISA flags so comparisons are fair.
        if(TARGET yyjson)
            target_compile_options(yyjson PRIVATE -march=native)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # (C) Non-Apple AArch64 + Clang: SVE SIGILL safety guards.
        # Clang generates SVE at LTO link time even when source only uses NEON.
        foreach(_tgt bench_all)
            if(TARGET ${_tgt})
                target_compile_options(${_tgt} PRIVATE
                    -fno-lto -fno-vectorize -fno-slp-vectorize)
                target_link_options(${_tgt} PRIVATE -fno-lto)
            endif()
        endforeach()
    endif()
    # GCC on Linux AArch64: -march=native is generally safe (GCC does not emit
    # SVE at LTO time for NEON-only sources), but omitted here for conservatism.
    # Enable explicitly when targeting known-SVE systems (e.g., Graviton 3).
endif()
