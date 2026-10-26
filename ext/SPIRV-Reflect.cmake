#
# Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(
    SPIRV-Reflect
    STATIC 
        SPIRV-Reflect/spirv_reflect.c
        SPIRV-Reflect/common/output_stream.cpp
)

target_include_directories(SPIRV-Reflect PUBLIC
    ./SPIRV-Reflect
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
target_compile_options(SPIRV-Reflect PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-overflow>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
endif()
