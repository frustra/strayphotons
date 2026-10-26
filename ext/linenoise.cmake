#
# Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#


# linenoise is not available on Win32
if (WIN32)
    return()
endif()

add_library(linenoise STATIC
    linenoise/linenoise.c
)

target_include_directories(linenoise PUBLIC
    linenoise
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    target_compile_options(linenoise PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-variadic-macro-arguments-omitted>)
endif()
