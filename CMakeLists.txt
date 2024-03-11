#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

# INTERPROCEDURAL_OPTIMIZATION works in 3.20
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
cmake_policy(SET CMP0048 NEW)
project(sp VERSION 0.1)

set(SP_VERSION_MAJOR "${CMAKE_PROJECT_VERSION_MAJOR}")
set(SP_VERSION_MINOR "${CMAKE_PROJECT_VERSION_MINOR}")
set(SP_VERSION "${SP_VERSION_MAJOR}.${SP_VERSION_MINOR}")

option(SP_PACKAGE_RELEASE "Build packaged release" OFF)
set(SP_BUILD_RUST_DEFAULT ON)
if(DEFINED ENV{SP_BUILD_RUST})
    set(SP_BUILD_RUST_DEFAULT $ENV{SP_BUILD_RUST})
endif()
option(SP_BUILD_RUST "Build and link against rust modules" ${SP_BUILD_RUST_DEFAULT})
option(SP_ENABLE_TRACY "Enable Tracy profiler" ON)

if(CMAKE_BUILD_TYPE MATCHES Release)
    message(STATUS "sp target is Release")
    set(SP_DEBUG 0)
elseif(CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS "sp target is Debug")
    set(SP_DEBUG 1)
    add_compile_definitions(SP_DEBUG=1)
elseif(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    message(STATUS "sp target is RelWithDebInfo")
    set(SP_DEBUG 0)
else()
    message(STATUS "sp target is default, using Debug")
    set(CMAKE_BUILD_TYPE Debug)
    set(SP_DEBUG 1)
    add_compile_definitions(SP_DEBUG=1)
endif()

if(SP_PACKAGE_RELEASE)
    message(STATUS "Enabling build for packaged release")
    add_compile_definitions(SP_PACKAGE_RELEASE=1)
    add_compile_definitions(CATCH_GLOBAL_EXCEPTIONS=1)
endif()
add_compile_definitions(_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS=1)

# Tell cmake we need C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## Setup compiler flags for various build types
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

    # warning C4100: '*': unreferenced formal parameter
    # warning C4201: nonstandard extension used: nameless struct/union
    # warning C4244: '=': conversion from '*' to '*', possible loss of data
    # warning C4267: 'argument': conversion from '*' to '*', possible loss of data
    # warning C4305: 'argument': truncation from '*' to '*'
    # warning C4389: '!=': signed/unsigned mismatch
    # warning C4390: ';': empty controlled statement found
    # warning C4458: declaration of '*' hides class member
    set(CMAKE_CXX_FLAGS                 "/MP /EHsc /W4 /WX /wd4100 /wd4201 /wd4244 /wd4267 /wd4305 /wd4389 /wd4390 /wd4458")
    set(CMAKE_CXX_FLAGS_RELEASE         "/MT /O2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "/MT /DEBUG /Zi /O2")
    set(CMAKE_CXX_FLAGS_DEBUG           "/MTd /DEBUG /Zi /FC")

    # Force targets to be built using static linking
    if(SP_DEBUG)
        set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebug)
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

    set(CMAKE_CXX_FLAGS                 "-Wall -Werror -Werror=format-security -Wno-volatile -fPIC")
    set(CMAKE_CXX_FLAGS_RELEASE         "-O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-g3 -ggdb3 -Og")
    set(CMAKE_CXX_FLAGS_DEBUG           "-g3 -ggdb3 -O0")

elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")

    set(CMAKE_CXX_FLAGS                 "-Wall -Werror -Wformat-security -Wno-deprecated-volatile -fgnuc-version=5 -fPIC")
    set(CMAKE_CXX_FLAGS_RELEASE         "-O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-g3 -ggdb3 -Og")
    set(CMAKE_CXX_FLAGS_DEBUG           "-g3 -ggdb3 -O0")

else()

    message(FATAL_ERROR "Unknown CXX compiler ${CMAKE_CXX_COMPILER_ID}")

endif()

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROJECT_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Put RUNTIME and LIBRARY outputs into a folder where we can run them without installing
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})

# For multi-config builds (e.g. msvc)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJECT_OUTPUT_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJECT_OUTPUT_DIR})
endforeach()

# This variable contains the component name used in calls to install() for the core stray photons
# installation
# Some external projects (like OpenVR) require it to be used in their install() calls
set(PROJECT_INSTALL sp)

# Root name for all libraries that will be part of this project
set(PROJECT_BASE_NAME strayphotons)

# Output shared library name (lib<name>.so / <name>.dll)
set(PROJECT_LIB_NAME sp)

# Interface library that is applied to all module configurations
set(PROJECT_COMMON_EXE ${PROJECT_BASE_NAME})
add_library(${PROJECT_COMMON_EXE} INTERFACE)
target_link_libraries(${PROJECT_COMMON_EXE} INTERFACE cxxopts)
target_include_directories(${PROJECT_COMMON_EXE}
    INTERFACE
        include
        src
)

if(UNIX)
    target_link_libraries(${PROJECT_COMMON_EXE} INTERFACE pthread)
elseif(WIN32)
    target_link_libraries(${PROJECT_COMMON_EXE} INTERFACE winmm)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    if(${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 8.0)
        message(FATAL_ERROR "Requires GCC >8.0")
    endif()

    target_link_libraries(${PROJECT_COMMON_EXE} INTERFACE stdc++fs)
endif()

# Build all external dependencies
add_subdirectory(ext)

if((CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64") OR (CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64"))
    set(ARM ON)
else()
    set(ARM OFF)
endif()

# Tell CMake to convert all relative paths in subdirectories to absolute paths
cmake_policy(SET CMP0076 NEW)

add_subdirectory(shaders)
add_subdirectory(shared)
add_subdirectory(src)
add_subdirectory(docs_generator)
add_subdirectory(hull_compiler)
add_subdirectory(scene_formatter)
add_subdirectory(assets)

if(SP_PACKAGE_RELEASE)
    if(UNIX)
        set_target_properties(${PROJECT_COMMON_EXE} PROPERTIES INSTALL_RPATH "\$ORIGIN")
        set_target_properties(${PROJECT_COMMON_EXE} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
    endif()
endif()

if(NOT SP_DEBUG AND NOT UNIX)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        message(STATUS "IPO / LTO enabled")
        set_target_properties(${PROJECT_COMMON_EXE} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE
        )
    else()
        message(STATUS "IPO / LTO not supported:\n${ipo_error}")
    endif()
endif()

if(SP_BUILD_RUST)
    message(STATUS "Building with Rust")
    add_subdirectory(rust)
    add_subdirectory(wasm)

    target_link_libraries(${PROJECT_COMMON_EXE} INTERFACE
        sp-rs-wasm
    )

    if(ipo_supported)
        set_target_properties(sp-rs-wasm PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT sp)

################################################
# Build tests
################################################
add_subdirectory(tests)
