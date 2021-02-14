if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build --verbose)
    set(CARGO_TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release --verbose)
    set(CARGO_TARGET_DIR "release")
endif ()

if(ENABLE_LTO)
    set(RUST_FLAGS "-Clinker-plugin-lto" "-Clinker=clang" "-Clink-arg=-fuse-ld=lld")
endif()

if (WIN32)
    set(CXXBRIDGE_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/sp_rust.lib")
else()
    set(CXXBRIDGE_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libsp_rust.a")
endif()

set(CXXBRIDGE_CC "${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rust/src/lib.rs.cc")

add_custom_command(COMMENT "Run cxxbridge codegen"
    OUTPUT ${CXXBRIDGE_LIB} ${CXXBRIDGE_CC}
    DEPENDS src/lib.rs build.rs Cargo.lock Cargo.toml
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(sp-rust STATIC ${CXXBRIDGE_CC})

target_include_directories(
    sp-rust
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rust/src
)

target_link_libraries(
    sp-rust
    PRIVATE
        ${CXXBRIDGE_LIB}
)

if(WIN32)
    target_link_libraries(sp-rust PRIVATE
        ws2_32
        userenv
    )
endif()
