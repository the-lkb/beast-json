cmake_minimum_required(VERSION 3.15)
project(qbuem_json_python)

# bindings/python/CMakeLists.txt — qbuem-json Python Bindings (nanobind + ctypes support)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

# Prefer the pip-installed nanobind (avoids FetchContent network dependency in CI).
# nanobind provides a helper: python -m nanobind --cmake_dir
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE NB_CMAKE_DIR
    RESULT_VARIABLE NB_CMAKE_RESULT
)

if(NB_CMAKE_RESULT EQUAL 0 AND NB_CMAKE_DIR)
    message(STATUS "Using pip-installed nanobind at: ${NB_CMAKE_DIR}")
    find_package(nanobind CONFIG REQUIRED HINTS "${NB_CMAKE_DIR}")
else()
    # Fallback: fetch from GitHub (for local development without pip install)
    message(STATUS "nanobind not found via pip — fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        nanobind
        GIT_REPOSITORY https://github.com/wjakob/nanobind
        GIT_TAG        v1.9.0
    )
    FetchContent_MakeAvailable(nanobind)
endif()

# The C++ library is header-only; we point to the root include directory.
set(QBUEM_JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

# 1. Native Extension (nanobind)
nanobind_add_module(qbuem_json_py qbuem_json_py.cpp)
target_include_directories(qbuem_json_py PRIVATE ${Python_INCLUDE_DIRS} ${QBUEM_JSON_INCLUDE_DIR})
target_compile_features(qbuem_json_py PRIVATE cxx_std_20)
target_link_libraries(qbuem_json_py PRIVATE ${Python_LIBRARIES})
set_target_properties(qbuem_json_py PROPERTIES OUTPUT_NAME "qbuem_json_native")

# 2. C Shared Library (for ctypes wrapper)
# We build it here as well so it can be bundled in the wheel.
add_library(qbuem_json_c_native SHARED ../c/qbuem_json_c.cpp)
target_include_directories(qbuem_json_c_native PRIVATE ${QBUEM_JSON_INCLUDE_DIR} ../c)
target_compile_features(qbuem_json_c_native PRIVATE cxx_std_20)
set_target_properties(qbuem_json_c_native PROPERTIES
    OUTPUT_NAME "qbuem_json_c")

# Install rules for scikit-build-core
# Both targets and the python wrapper go into the 'qbuem_json' package directory.
install(TARGETS qbuem_json_py qbuem_json_c_native
        LIBRARY DESTINATION qbuem_json
        RUNTIME DESTINATION qbuem_json
        COMPONENT python_bindings)

install(FILES qbuem_json.py
        DESTINATION qbuem_json
        RENAME __init__.py
        COMPONENT python_bindings)

message(STATUS "qbuem-json Python bindings (nanobind + ctypes) configured with install rules")
