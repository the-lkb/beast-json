# bindings/CMakeLists.txt — qbuem-json Language Bindings

# C Shared Library
add_subdirectory(c)

# Python Bindings (nanobind)
option(QBUEM_JSON_BUILD_PYTHON "Build Python bindings" ON)
if(QBUEM_JSON_BUILD_PYTHON)
    add_subdirectory(python)
endif()

# Rust Bindings (cxx is typically built via cargo, but we can add a stub)
# add_subdirectory(rust)
