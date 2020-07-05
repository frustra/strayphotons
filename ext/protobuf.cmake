set(protobuf_WITH_ZLIB OFF CACHE BOOL "Build protobuf without zlib" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "Build protobuf without tests" FORCE)
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "Build protobuf with /MD" FORCE)

add_subdirectory(protobuf/cmake EXCLUDE_FROM_ALL)

target_include_directories(${PROJECT_LIB} PUBLIC "${PROJECT_SOURCE_DIR}/ext/protobuf/src")
