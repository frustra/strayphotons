if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build --verbose)
    set(CARGO_TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release --verbose)
    set(CARGO_TARGET_DIR "release")
endif ()

if(WIN32)
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
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(sp-rust STATIC ${CXXBRIDGE_CC})

target_include_directories(sp-rust
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rust/src
)

# Adding dependencies here will automatically setup include directories for Rust
target_link_libraries(sp-rust PRIVATE
    ${CXXBRIDGE_LIB}
    ${PROJECT_CORE_LIB}
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
