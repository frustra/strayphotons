# INTERPROCEDURAL_OPTIMIZATION works in 3.20
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
cmake_policy(SET CMP0048 NEW)
project(sp VERSION 0.1)

set(SP_VERSION_MAJOR "${CMAKE_PROJECT_VERSION_MAJOR}")
set(SP_VERSION_MINOR "${CMAKE_PROJECT_VERSION_MINOR}")
set(SP_VERSION "${SP_VERSION_MAJOR}.${SP_VERSION_MINOR}")

option(SP_PACKAGE_RELEASE "Build packaged release" off)
option(SP_NO_MODEL_COMPILE "Disable the model compiler, which needs Node.js" off)
option(SP_BUILD_RUST "Build and link against rust modules" on)

if(CMAKE_BUILD_TYPE MATCHES Release)
    message(STATUS "sp target is Release")
    set(SP_DEBUG 0)
elseif(CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS "sp target is Debug")
    set(SP_DEBUG 1)
    add_compile_definitions(SP_DEBUG=1)
else()
    message(STATUS "sp target is default, using Debug")
    set(CMAKE_BUILD_TYPE Debug)
    set(SP_DEBUG 1)
    add_compile_definitions(SP_DEBUG=1)
endif()

if(SP_PACKAGE_RELEASE)
    message(STATUS "Enabling build for packaged release")
    add_compile_definitions(SP_PACKAGE_RELEASE=1)
endif()

# Tell cmake to export a compile_commands.json file
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Tell cmake we need C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## Setup compiler flags for various build types
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

    # TODO: Enable warnings as errors on MSVC
    set(CMAKE_CXX_FLAGS 				"/MP /EHsc") # /Wall /WX") # /wd4800 /wd4189
    set(CMAKE_CXX_FLAGS_RELEASE 		"/MT /O2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 	"/MTd /O2")
    set(CMAKE_CXX_FLAGS_DEBUG 			"/MTd /DEBUG /ZI /FC")

    # Force targets to be built using static linking
    if(SP_DEBUG)
        set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebug)
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

    set(CMAKE_CXX_FLAGS 				"-Wall -Werror -Werror=format-security -Wno-volatile")
    set(CMAKE_CXX_FLAGS_RELEASE 		"-O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 	"-g3 -ggdb3 -Og")
    set(CMAKE_CXX_FLAGS_DEBUG 			"-g3 -ggdb3 -O0")

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

    set(CMAKE_CXX_FLAGS 				"-Wall -Werror -Wformat-security -Wno-deprecated-volatile")
    set(CMAKE_CXX_FLAGS_RELEASE 		"-O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 	"-g3 -ggdb3 -Og")
    set(CMAKE_CXX_FLAGS_DEBUG 			"-g3 -ggdb3 -O0")

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
# Some external projects (like OpenVR and fmod) require it to be used in their install() calls
set(PROJECT_INSTALL sp)

# Root name for all libraries that will be part of this project
set(PROJECT_BASE_NAME strayphotons)

# Interface library that is applied to all module configurations
set(PROJECT_COMMON_EXE ${PROJECT_BASE_NAME})
add_library(${PROJECT_COMMON_EXE} INTERFACE)
target_link_libraries(${PROJECT_COMMON_EXE} INTERFACE cxxopts)
target_include_directories(${PROJECT_COMMON_EXE} INTERFACE src)

if(UNIX)
    target_link_libraries(${PROJECT_COMMON_EXE} INTERFACE pthread)
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

add_subdirectory(assets)
add_subdirectory(shaders)
add_subdirectory(src)

if(SP_PACKAGE_RELEASE)
    if(UNIX)
        set_target_properties(${PROJECT_COMMON_EXE} PROPERTIES INSTALL_RPATH "\$ORIGIN")
        set_target_properties(${PROJECT_COMMON_EXE} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
    endif()
endif()

if(NOT SP_DEBUG)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        message(STATUS "IPO / LTO enabled")
        set_target_properties(${PROJECT_COMMON_EXE} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE
        )
    else()
        message(STATUS "IPO / LTO not supported: <${ipo_error}>")
    endif()
endif()

if(SP_BUILD_RUST)
    add_subdirectory(rust)

    target_compile_definitions(${PROJECT_COMMON_EXE} INTERFACE
        RUST_CXX
    )

    target_link_libraries(${PROJECT_COMMON_EXE} INTERFACE
        sp-rust
    )
    
    if(ipo_supported)
        set_target_properties(sp-rust PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT sp)

################################################
# Build tests
################################################
add_subdirectory(tests)
