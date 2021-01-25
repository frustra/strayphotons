
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(CARGO_CMD cargo build --verbose)
set(TARGET_DIR "debug")
else ()
set(CARGO_CMD cargo build --release --verbose)
set(TARGET_DIR "release")
endif ()

if(ENABLE_LTO)
set(RUST_FLAGS "-Clinker-plugin-lto" "-Clinker=clang-11" "-Clink-arg=-fuse-ld=lld-11")
endif()

set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/cargo/${TARGET_DIR}/libscreen.a")
set(SCREEN_CXX "${CMAKE_CURRENT_BINARY_DIR}/cargo/screen.cpp")

add_library(screen STATIC ${SCREEN_CXX})

target_link_libraries(
    screen
    STATIC
        pthread
        dl
        ${SCREEN_LIB}
)

add_custom_target(screen_lib ALL
    COMMENT "Compiling screen module"
    COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo/ RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen/src/lib.rs.cc ${SCREEN_CXX}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cargo/cxxbridge/screen/src/lib.rs.h ${CMAKE_CURRENT_BINARY_DIR}/cargo/screen.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/screen
    BYPRODUCTS ${SCREEN_LIB})

add_dependencies(${SCREEN_LIB} screen_lib)
add_dependencies(${PROJECT_LIB} screen_lib)

add_test(NAME screen_test 
    COMMAND cargo test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/screen)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build --verbose)
    set(TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release --verbose)
    set(TARGET_DIR "release")
endif ()

if(ENABLE_LTO)
    set(RUST_FLAGS "-Clinker-plugin-lto" "-Clinker=clang-11" "-Clink-arg=-fuse-ld=lld-11")
endif()

set(SCREEN_LIB "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_DIR}/libscreen.a")

set(SCREEN_CXX "${CMAKE_CURRENT_BINARY_DIR}/screen.cpp")
add_library(screen STATIC ${SCREEN_CXX})
add_custom_command(
    OUTPUT ${SCREEN_CXX}
    COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR} RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/screen/src/lib.rs.cc ${SCREEN_CXX}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/screen/src/lib.rs.h ${CMAKE_CURRENT_SOURCE_DIR}/screen/screen.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(screen pthread dl ${SCREEN_LIB})