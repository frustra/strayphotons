MESSAGE("Added screen-rs")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build --verbose)
    set(CARGO_TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release --verbose)
    set(CARGO_TARGET_DIR "release")
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/screen-rs/src)
include_directories(${CMAKE_SOURCE_DIR}/src/graphics)
include_directories(${CMAKE_SOURCE_DIR}/src/graphics/postprocess)

if(ENABLE_LTO)
    set(RUST_FLAGS "-Clinker-plugin-lto" "-Clinker=clang" "-Clink-arg=-fuse-ld=lld")
endif()

if (WIN32)
    set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libscreen_rs.dll")
else()
    set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libscreen_rs.a")
endif()

set(SCREEN_CXX "${CMAKE_CURRENT_SOURCE_DIR}/screen-rs/target/cxxbridge/screen-rs/src/lib.rs.cc")
set(SCREEN_HXX "${CMAKE_CURRENT_SOURCE_DIR}/screen-rs/target/cxxbridge/screen-rs/src/lib.rs.h")

add_library(screen-rs STATIC ${SCREEN_LIB})

target_link_libraries(
    screen-rs
        ${SCREEN_LIB}
)

set_target_properties(screen-rs PROPERTIES LINKER_LANGUAGE CXX)

find_program(CXXBRIDGE cxxbridge REQUIRED PATHS $ENV{HOME}/.cargo/bin)
message(STATUS "Using cxxbridge: ${CXXBRIDGE}")

add_custom_target(screen_lib ALL
    COMMENT "Compiling screen module"
    OUTPUT ${SCREEN_CXX}
    COMMAND cargo install cxxbridge-cmd
    COMMAND cxxbridge --header --output "${CMAKE_CURRENT_SOURCE_DIR}/screen-rs/target/cxxbridge/rust/cxx.h"
    COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen-rs/src/lib.rs.cc ${SCREEN_CXX}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen-rs/src/lib.rs.h ${SCREEN_HXX}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/screen-rs
    BYPRODUCTS ${SCREEN_LIB})

add_dependencies(${PROJECT_LIB} screen_lib)

add_test(NAME screen_test 
    COMMAND cargo test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/screen-rs)