#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
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
    ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Reflect
)
