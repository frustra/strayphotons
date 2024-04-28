#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(microtar STATIC microtar/src/microtar.c)

target_include_directories(
    microtar
    PUBLIC
        microtar/src
)

if(WIN32)
    target_compile_definitions(
        microtar
        PUBLIC
            _CRT_SECURE_NO_WARNINGS
    )
endif()
