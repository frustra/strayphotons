#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(BUILD_EXAMPLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_DYNAMIC_LIBS OFF CACHE BOOL "" FORCE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/wd4061 /wd4100 /wd4189 /wd4211 /wd4242 /wd4244 /wd4267 /wd4365 /wd4456 /wd4577 /wd4623 /wd4625)
    add_compile_options(/wd4626 /wd4668 /wd4706 /wd4710 /wd4800 /wd4820 /wd4996 /wd5026 /wd5027 /wd5045 /wd5219 /wd5246)

    foreach(flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG)
        STRING (REGEX REPLACE "/RTC[^ ]*" "" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)
endif()

add_subdirectory(libsoundio) # target: libsoundio_static
add_compile_definitions(SOUNDIO_STATIC_LIBRARY)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(libsoundio_static PRIVATE "/O2")
endif()

target_include_directories(
    libsoundio_static
    INTERFACE
        ./libsoundio/
)
