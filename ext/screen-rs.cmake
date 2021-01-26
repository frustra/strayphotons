
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(CARGO_CMD cargo build --verbose)
set(CARGO_TARGET_DIR "debug")
else ()
set(CARGO_CMD cargo build --release --verbose)
set(CARGO_TARGET_DIR "release")
endif ()

if(ENABLE_LTO)
set(RUST_FLAGS "-Clinker-plugin-lto" "-Clinker=clang-11" "-Clink-arg=-fuse-ld=lld-11")
endif()

if (WIN32)
    set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libscreen-rs.dll")
else()
    set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libscreen-rs.a")
endif()

set(SCREEN_CXX "${CMAKE_CURRENT_SOURCE_DIR}/screen-rs/src/Api.cc")

add_library(screen-rs STATIC ${SCREEN_CXX})

target_link_libraries(
    screen-rs
    STATIC
        pthread
        dl
        ${SCREEN_LIB}
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/screen-rs/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../graphics)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../graphics/postprocess)

add_custom_target(screen_lib ALL
    COMMENT "Compiling screen module"
    COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen-rs/src/lib.rs.cc ${SCREEN_CXX}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen-rs/src/lib.rs.h ${CMAKE_CURRENT_SOURCE_DIR}/screen-rs/src/Screen.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/screen-rs
    BYPRODUCTS ${SCREEN_LIB})

add_dependencies(${PROJECT_LIB} screen_lib)

add_test(NAME screen_test 
    COMMAND cargo test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/screen-rs)