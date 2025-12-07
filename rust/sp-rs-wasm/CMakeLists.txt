#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(CXXBRIDGE_WASM_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_BUILD_TYPE}")
if(WIN32)
    set(RUST_OUTPUT_LIB "sp_rs_wasm.lib")
else()
    set(RUST_OUTPUT_LIB "libsp_rs_wasm.a")
endif()

# Add rust source files here, and in build.rs
set(RUST_SOURCE_FILES
    src/lib.rs
    src/wasm.rs
)

set(CXXBRIDGE_WASM_SP_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rs-wasm/src")
add_custom_command(COMMENT "Run wasm cxxbridge codegen"
    OUTPUT
        "${CXXBRIDGE_WASM_DIR}/${RUST_OUTPUT_LIB}"
        "${CXXBRIDGE_WASM_SP_DIR}/wasm.rs.cc"
        "${CXXBRIDGE_WASM_SP_DIR}/wasm.rs.h"
    DEPENDS ${RUST_SOURCE_FILES} build.rs Cargo.toml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}

    COMMAND ${CMAKE_COMMAND} -E env
        CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/
        CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        RUST_INCLUDES="$<TARGET_PROPERTY:sp-rs-wasm,INTERFACE_INCLUDE_DIRECTORIES>"
        ${CARGO_CMD} --no-default-features --features "sp-rs-wasm/wasm"
)

add_custom_target(sp-rs-wasm-build DEPENDS
    "${CXXBRIDGE_WASM_DIR}/${RUST_OUTPUT_LIB}"
    "${CXXBRIDGE_WASM_SP_DIR}/wasm.rs.h"
)
add_library(sp-rs-wasm INTERFACE)
add_dependencies(sp-rs-wasm sp-rs-wasm-build)

target_include_directories(sp-rs-wasm
    INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rs-wasm/src
        ${PROJECT_ROOT_DIR}/rust
)

# Adding dependencies here will automatically setup include directories for Rust
target_link_libraries(sp-rs-wasm INTERFACE
    "${CXXBRIDGE_WASM_DIR}/${RUST_OUTPUT_LIB}"
    ${PROJECT_CORE_LIB}
    Tecs
)
target_compile_definitions(sp-rs-wasm INTERFACE
    SP_RUST_WASM_SUPPORT
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
elseif(UNIX)
    target_link_libraries(sp-rs-wasm INTERFACE
        pthread
        dl
    )
endif()
