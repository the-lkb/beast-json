cmake_minimum_required(VERSION 3.14)
project(beast_json VERSION 1.0.3 LANGUAGES CXX)

# Library Target
add_library(beast_json INTERFACE)
add_library(beast_json::beast_json ALIAS beast_json)

# Include Directories
target_include_directories(beast_json INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external> # For sse2neon.h
    $<INSTALL_INTERFACE:include>
)

# Compile Features
target_compile_features(beast_json INTERFACE cxx_std_20)

# Install Rules
include(GNUInstallDirs)
install(TARGETS beast_json
    EXPORT beast_jsonTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES external/sse2neon.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/beast_json/external)

# Export Configuration
install(EXPORT beast_jsonTargets
    FILE beast_jsonTargets.cmake
    NAMESPACE beast_json::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/beast_json
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "beast_jsonConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "beast_jsonConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/beast_json
)

# Tests (Optional)
option(BEAST_JSON_BUILD_TESTS "Build tests" ON)
if(BEAST_JSON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks (Optional)
option(BEAST_JSON_BUILD_BENCHMARKS "Build benchmarks" ON)
if(BEAST_JSON_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Fuzz targets (Optional – requires Clang + libclang-rt-dev)
option(BEAST_JSON_BUILD_FUZZ "Build libFuzzer fuzz targets (requires Clang)" OFF)
if(BEAST_JSON_BUILD_FUZZ)
    add_subdirectory(fuzz)
endif()

# Language bindings (Optional)
option(BEAST_JSON_BUILD_BINDINGS "Build C shared library + language bindings" OFF)
if(BEAST_JSON_BUILD_BINDINGS)
    add_subdirectory(bindings/c)
endif()
