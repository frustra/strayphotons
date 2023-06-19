#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build --lib --verbose)
    set(CARGO_TARGET_DIR "debug")
else()
    set(CARGO_CMD cargo build --lib --release --verbose)
    set(CARGO_TARGET_DIR "release")
endif()

if(WIN32)
    set(RUST_FLAGS "-C" "target-feature=+crt-static")
    set(CXXBRIDGE_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/sp_rust.lib")
else()
    set(CXXBRIDGE_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libsp_rust.a")
endif()

# Add rust source files here, and in build.rs
set(RUST_SOURCE_FILES
    src/lib.rs
    src/wasmer_vm.rs
)

set(CXXBRIDGE_CC "${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rust/src/lib.rs.cc")
file(GENERATE OUTPUT rust_includes.list CONTENT "$<TARGET_PROPERTY:sp-rust,INCLUDE_DIRECTORIES>")
add_custom_command(COMMENT "Run cxxbridge codegen"
    OUTPUT ${CXXBRIDGE_LIB} ${CXXBRIDGE_CC}
    DEPENDS ${RUST_SOURCE_FILES} build.rs Cargo.toml ${CMAKE_CURRENT_BINARY_DIR}/rust_includes.list
    COMMAND ${CMAKE_COMMAND} -E env RUSTFLAGS="${RUST_FLAGS}" CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ ${CARGO_CMD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(sp-rust STATIC ${CXXBRIDGE_CC})

target_include_directories(sp-rust
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rust/src
)

target_compile_definitions(sp-rust PUBLIC RUST_CXX)

# Adding dependencies here will automatically setup include directories for Rust
target_link_libraries(sp-rust PRIVATE
    ${CXXBRIDGE_LIB}
    ${PROJECT_CORE_LIB}
    ${PROJECT_SCRIPTS_LIB}
    Tecs
)

if(WIN32)
    target_link_libraries(sp-rust PRIVATE
        ws2_32
        userenv
        bcrypt
    )
elseif(UNIX)
    target_link_libraries(sp-rust PRIVATE
        pthread
        dl
    )
endif()
