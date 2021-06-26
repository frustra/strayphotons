if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build --verbose)
    set(CARGO_TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release --verbose)
    set(CARGO_TARGET_DIR "release")
endif ()

if (WIN32)
    set(CXXBRIDGE_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/sp_rust.lib")
else()
    set(CXXBRIDGE_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libsp_rust.a")
endif()

set(CXXBRIDGE_CC "${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rust/src/lib.rs.cc")

string(JOIN ";" RUST_INCLUDE_LIST
    ${PROJECT_ROOT_DIR}/ext/Tecs/inc # $<TARGET_PROPERTY:Tecs,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_ROOT_DIR}/src/core
)

add_custom_command(COMMENT "Run cxxbridge codegen"
    OUTPUT ${CXXBRIDGE_LIB} ${CXXBRIDGE_CC}
    DEPENDS src/lib.rs build.rs Cargo.lock Cargo.toml
    COMMAND ${CMAKE_COMMAND} -E env CMAKE_ADDITIONAL_INCLUDES="${RUST_INCLUDE_LIST}" CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(sp-rust STATIC ${CXXBRIDGE_CC})

target_include_directories(
    sp-rust
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/sp-rust/src
    PRIVATE
        ${PROJECT_ROOT_DIR}/src/core
)

target_link_libraries(
    sp-rust
    PRIVATE
        ${CXXBRIDGE_LIB}
        Tecs
)

if(WIN32)
    target_link_libraries(sp-rust PRIVATE
        ws2_32
        userenv
    )
endif()
