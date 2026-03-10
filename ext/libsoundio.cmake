#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(BUILD_EXAMPLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_DYNAMIC_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(libsoundio)
target_compile_definitions(libsoundio_static PUBLIC SOUNDIO_STATIC_LIBRARY)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(libsoundio_static PRIVATE /O2)
    target_compile_options(libsoundio_static PRIVATE /wd4061 /wd4100 /wd4189 /wd4211 /wd4242 /wd4244 /wd4267 /wd4365 /wd4456 /wd4577 /wd4623 /wd4625)
    target_compile_options(libsoundio_static PRIVATE /wd4626 /wd4668 /wd4706 /wd4710 /wd4800 /wd4820 /wd4996 /wd5026 /wd5027 /wd5045 /wd5219 /wd5246)

    foreach(flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG)
        STRING (REGEX REPLACE "/RTC[^ ]*" "" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(libsoundio_static PRIVATE -O3 -Wno-unused-variable)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    target_compile_options(libsoundio_static PRIVATE -O3 -Wno-unused-private-field)
endif()

if(UNIX)
    target_link_libraries(libsoundio_static INTERFACE jack pulse asound)
endif()

target_include_directories(
    libsoundio_static
    INTERFACE
        ./libsoundio/
)
