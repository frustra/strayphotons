#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build --verbose)
    set(CARGO_TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release --verbose)
    set(CARGO_TARGET_DIR "release")
endif ()

if(WIN32)
    set(CXXBRIDGE_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/sp_rs.lib")
else()
    set(CXXBRIDGE_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libsp_rs.a")
endif()

# Add rust source files here, and in build.rs
set(RUST_SOURCE_FILES
    src/api.rs
    src/gfx.rs
    src/lib.rs
    src/wasm.rs
)

set(CXXBRIDGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rs/src")
file(GENERATE OUTPUT rust_includes.list CONTENT "$<TARGET_PROPERTY:sp-rs,INCLUDE_DIRECTORIES>")
add_custom_command(COMMENT "Run cxxbridge codegen"
    OUTPUT ${CXXBRIDGE_LIB} "${CXXBRIDGE_DIR}/api.rs.cc" "${CXXBRIDGE_DIR}/gfx.rs.cc" "${CXXBRIDGE_DIR}/wasm.rs.cc"
    DEPENDS ${RUST_SOURCE_FILES} build.rs Cargo.toml ${CMAKE_CURRENT_BINARY_DIR}/rust_includes.list
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ ${CARGO_CMD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(sp-rs STATIC 
    "${CXXBRIDGE_DIR}/api.rs.cc"
    "${CXXBRIDGE_DIR}/gfx.rs.cc"
    "${CXXBRIDGE_DIR}/wasm.rs.cc"
)

target_include_directories(sp-rs
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rs/src
)

target_include_directories(${PROJECT_GRAPHICS_CORE_LIB} 
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rs/src
)

# Adding dependencies here will automatically setup include directories for Rust
target_link_libraries(sp-rs PRIVATE
    ${CXXBRIDGE_LIB}
    ${PROJECT_CORE_LIB}
    Tecs
)

if(WIN32)
    target_link_libraries(sp-rs PRIVATE
        ws2_32
        userenv
        bcrypt
        ntdll
        Dwmapi
        UxTheme
    )
elseif(UNIX)
    target_link_libraries(sp-rs PRIVATE
        pthread
        dl
    )
endif()
