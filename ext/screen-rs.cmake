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
else()
    set(RUST_FLAGS "-Clinker=clang")
endif()

if (WIN32)
    set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libscreen_rs.dll")
else()
    set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libscreen_rs.a")
endif()

set(SCREEN_LIB_D "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libscreen_rs.d")
set(SCREEN_CXX "${CMAKE_CURRENT_SOURCE_DIR}/screen-rs/src/Screen.cc")
set(SCREEN_HXX "${CMAKE_CURRENT_SOURCE_DIR}/screen-rs/src/Screen.hh")

add_library(screen-rs STATIC ${SCREEN_LIB})

target_link_libraries(
    screen-rs
        pthread
        dl
        ${SCREEN_LIB}
        ${SCREEN_LIB_D}
)

set_target_properties(screen-rs PROPERTIES LINKER_LANGUAGE CXX)

add_custom_target(screen_lib ALL
    COMMENT "Compiling screen module"
    OUTPUT ${SCREEN_CXX}
    COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen-rs/src/lib.rs.cc ${SCREEN_CXX}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen-rs/src/lib.rs.h ${SCREEN_HXX}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/screen-rs
    BYPRODUCTS ${SCREEN_LIB} ${SCREEN_LIB_D})

add_dependencies(${PROJECT_LIB} screen_lib)

add_test(NAME screen_test 
    COMMAND cargo test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/screen-rs)