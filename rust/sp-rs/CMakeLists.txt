#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(CXXBRIDGE_WASM_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo-wasm/${CARGO_TARGET_DIR}")
set(CXXBRIDGE_WINDOW_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo-window/${CARGO_TARGET_DIR}")
if(WIN32)
    set(RUST_OUTPUT_LIB "sp_rs.lib")
else()
    set(RUST_OUTPUT_LIB "libsp_rs.a")
endif()

# Add rust source files here, and in build.rs
set(RUST_SOURCE_FILES
    src/api.rs
    src/lib.rs
    src/wasm.rs
    src/window.rs
)

set(CXXBRIDGE_WASM_SP_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo-wasm/cxxbridge/sp-rs/src")
add_custom_command(COMMENT "Run wasm cxxbridge codegen"
    OUTPUT
        ${CXXBRIDGE_WASM_DIR}/${RUST_OUTPUT_LIB}
        "${CXXBRIDGE_WASM_SP_DIR}/api.rs.cc"
        "${CXXBRIDGE_WASM_SP_DIR}/api.rs.h"
        "${CXXBRIDGE_WASM_SP_DIR}/wasm.rs.cc"
        "${CXXBRIDGE_WASM_SP_DIR}/wasm.rs.h"
    DEPENDS ${RUST_SOURCE_FILES} build.rs Cargo.toml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}

    COMMAND ${CMAKE_COMMAND} -E env
        CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo-wasm/
        RUST_INCLUDES="$<TARGET_PROPERTY:sp-rs-wasm,INCLUDE_DIRECTORIES>"
        ${CARGO_CMD} --no-default-features --features "sp-rs/api,sp-rs/wasm"
)

set(CXXBRIDGE_WINDOW_SP_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo-window/cxxbridge/sp-rs/src")
add_custom_command(COMMENT "Run window cxxbridge codegen"
    OUTPUT
        ${CXXBRIDGE_WINDOW_DIR}/${RUST_OUTPUT_LIB}
        "${CXXBRIDGE_WINDOW_SP_DIR}/api.rs.cc"
        "${CXXBRIDGE_WINDOW_SP_DIR}/api.rs.h"
        "${CXXBRIDGE_WINDOW_SP_DIR}/wasm.rs.cc"
        "${CXXBRIDGE_WINDOW_SP_DIR}/wasm.rs.h"
        "${CXXBRIDGE_WINDOW_SP_DIR}/window.rs.cc"
        "${CXXBRIDGE_WINDOW_SP_DIR}/window.rs.h"
    DEPENDS ${RUST_SOURCE_FILES} build.rs Cargo.toml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}

    COMMAND ${CMAKE_COMMAND} -E env
        CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo-window/
        RUST_INCLUDES="$<TARGET_PROPERTY:sp-rs-window,INCLUDE_DIRECTORIES>"
        ${CARGO_CMD} --no-default-features --features "sp-rs/api,sp-rs/wasm,sp-rs/window"
)

add_library(sp-rs-wasm INTERFACE)
add_library(sp-rs-window INTERFACE)

target_include_directories(sp-rs-wasm
    INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rs/src
        ${PROJECT_ROOT_DIR}/rust
)

target_include_directories(sp-rs-window
    INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rs/src
        ${PROJECT_ROOT_DIR}/rust
        ${PROJECT_ROOT_DIR}/src/input
)

# Adding dependencies here will automatically setup include directories for Rust
target_link_libraries(sp-rs-wasm INTERFACE
    ${CXXBRIDGE_WASM_DIR}/${RUST_OUTPUT_LIB}
    ${PROJECT_CORE_LIB}
    Tecs
)
target_link_libraries(sp-rs-window INTERFACE
    ${CXXBRIDGE_WINDOW_DIR}/${RUST_OUTPUT_LIB}
    ${PROJECT_CORE_LIB}
    Tecs
)

if(WIN32)
    target_link_libraries(sp-rs-wasm INTERFACE
        ws2_32
        userenv
        bcrypt
        ntdll
        Dwmapi
        UxTheme
    )
    target_link_libraries(sp-rs-window INTERFACE
        ws2_32
        userenv
        bcrypt
        ntdll
        Dwmapi
        UxTheme
    )
elseif(UNIX)
    target_link_libraries(sp-rs-wasm INTERFACE
        pthread
        dl
    )
    target_link_libraries(sp-rs-window INTERFACE
        pthread
        dl
    )
endif()
