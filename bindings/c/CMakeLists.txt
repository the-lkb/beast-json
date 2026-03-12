# bindings/c/CMakeLists.txt — qbuem-json C Shared Library

add_library(qbuem_json_c SHARED qbuem_json_c.cpp)
target_link_libraries(qbuem_json_c PRIVATE qbuem_json::qbuem_json)
target_compile_features(qbuem_json_c PRIVATE cxx_std_20)
target_include_directories(qbuem_json_c
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>)

set_target_properties(qbuem_json_c PROPERTIES
    VERSION   1.0.0
    SOVERSION 1
    PUBLIC_HEADER qbuem_json_c.h)

# Install targets
install(TARGETS qbuem_json_c
    LIBRARY       DESTINATION lib
    PUBLIC_HEADER DESTINATION include)

message(STATUS "qbuem_json_c shared library target configured")
