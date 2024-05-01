#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
set(CXXBRIDGE_WINIT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_BUILD_TYPE}")
if(WIN32)
    set(RUST_OUTPUT_LIB "sp_rs_winit.lib")
else()
    set(RUST_OUTPUT_LIB "libsp_rs_winit.a")
endif()

# Add rust source files here, and in build.rs
set(RUST_SOURCE_FILES
    src/winit.rs
)

set(CXXBRIDGE_WINIT_SP_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rs-winit/src")
add_custom_command(COMMENT "Run winit cxxbridge codegen"
    OUTPUT
        "${CXXBRIDGE_WINIT_DIR}/${RUST_OUTPUT_LIB}"
        "${CXXBRIDGE_WINIT_SP_DIR}/winit.rs.cc"
        "${CXXBRIDGE_WINIT_SP_DIR}/winit.rs.h"
    DEPENDS ${RUST_SOURCE_FILES} build.rs Cargo.toml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}

    COMMAND ${CMAKE_COMMAND} -E env
        CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/
        CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        RUST_INCLUDES="$<TARGET_PROPERTY:sp-rs-winit,INTERFACE_INCLUDE_DIRECTORIES>"
        ${CARGO_CMD} --no-default-features
)

add_custom_target(sp-rs-winit-build DEPENDS
    "${CXXBRIDGE_WINIT_DIR}/${RUST_OUTPUT_LIB}"
    "${CXXBRIDGE_WINIT_SP_DIR}/winit.rs.h"
)
add_library(sp-rs-winit INTERFACE)
add_dependencies(sp-rs-winit sp-rs-winit-build)

target_include_directories(sp-rs-winit
    INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rs-winit/src
        ${PROJECT_ROOT_DIR}/rust
        ${PROJECT_ROOT_DIR}/src/input
)

# Adding dependencies here will automatically setup include directories for Rust
target_link_libraries(sp-rs-winit INTERFACE
    "${CXXBRIDGE_WINIT_DIR}/${RUST_OUTPUT_LIB}"
    ${PROJECT_COMMON_LIB}
)
target_compile_definitions(sp-rs-winit INTERFACE
    SP_RUST_WINIT_SUPPORT
)

if(WIN32)
    target_link_libraries(sp-rs-winit INTERFACE
        ws2_32
        userenv
        bcrypt
        ntdll
        Dwmapi
        UxTheme
    )
elseif(UNIX)
    target_link_libraries(sp-rs-winit INTERFACE
        pthread
        dl
    )
endif()
