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
    set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/screen_rs.lib")
else()
    set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${CARGO_TARGET_DIR}/libscreen_rs.a")
endif()

set(SCREEN_CXX "${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen-rs/src/lib.rs.cc")

add_custom_target(screen_lib ALL
    COMMENT "Compiling screen module"
    OUTPUT ${SCREEN_CXX}
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    BYPRODUCTS ${SCREEN_LIB} ${SCREEN_CXX}
)

add_library(screen-rs STATIC ${SCREEN_CXX})

target_include_directories(
    screen-rs
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen-rs/src
        ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(UNIX)
    target_link_libraries(
        screen-rs
        PRIVATE
            rt
    )
elseif(WIN32)
    target_link_libraries(
        screen-rs
        PRIVATE
            ws2_32
            userenv
            dwmapi
            comctl32
            uxtheme
            d3dcompiler
    )
endif()

target_link_libraries(
    screen-rs
    PRIVATE
        ${SCREEN_LIB}
)

add_dependencies(screen-rs screen_lib)
set_target_properties(screen-rs PROPERTIES LINKER_LANGUAGE CXX)

add_test(NAME screen_test 
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ cargo test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
