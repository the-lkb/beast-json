cmake_minimum_required(VERSION 3.15)
project(qbuem_json_python)

# bindings/python/CMakeLists.txt — qbuem-json Python Bindings (nanobind)

include(FetchContent)
FetchContent_Declare(
    nanobind
    GIT_REPOSITORY https://github.com/wjakob/nanobind
    GIT_TAG        v1.9.0
)
FetchContent_MakeAvailable(nanobind)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

nanobind_add_module(qbuem_json_py qbuem_json_py.cpp)
# target_link_libraries(qbuem_json_py PRIVATE qbuem_json::qbuem_json)
target_include_directories(qbuem_json_py PRIVATE ${Python_INCLUDE_DIRS} ../../include)
target_compile_features(qbuem_json_py PRIVATE cxx_std_20)

# Set output name to avoid collision/confusion
set_target_properties(qbuem_json_py PROPERTIES OUTPUT_NAME "qbuem_json_native")

message(STATUS "qbuem-json Python (nanobind) extension configured")
