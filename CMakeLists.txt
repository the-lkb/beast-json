cmake_minimum_required(VERSION 3.14)
project(qbuem_json VERSION 1.0.6 LANGUAGES CXX)

# Library Target
add_library(qbuem_json INTERFACE)
add_library(qbuem_json::qbuem_json ALIAS qbuem_json)

# Include Directories
target_include_directories(qbuem_json INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Compile Features
target_compile_features(qbuem_json INTERFACE cxx_std_20)

# Install Rules
include(GNUInstallDirs)
install(TARGETS qbuem_json
    EXPORT qbuem_jsonTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Export Configuration
install(EXPORT qbuem_jsonTargets
    FILE qbuem_jsonTargets.cmake
    NAMESPACE qbuem_json::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qbuem_json
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "qbuem_jsonConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "qbuem_jsonConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qbuem_json
)

# Tests (Optional)
option(QBUEM_JSON_BUILD_TESTS "Build tests" ON)
if(QBUEM_JSON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks (Optional)
option(QBUEM_JSON_BUILD_BENCHMARKS "Build benchmarks" ON)
if(QBUEM_JSON_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Fuzz targets (Optional – requires Clang + libclang-rt-dev)
option(QBUEM_JSON_BUILD_FUZZ "Build libFuzzer fuzz targets (requires Clang)" OFF)
if(QBUEM_JSON_BUILD_FUZZ)
    add_subdirectory(fuzz)
endif()

# Language bindings (Optional)
option(QBUEM_JSON_BUILD_BINDINGS "Build C shared library + language bindings" OFF)
if(QBUEM_JSON_BUILD_BINDINGS)
    add_subdirectory(bindings)
endif()
