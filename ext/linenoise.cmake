#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#


# linenoise is not available on Win32
if (WIN32)
    return()
endif()

add_library(
    linenoise 
    STATIC 
        linenoise/linenoise.c
)

target_include_directories(
    linenoise
    PUBLIC
        linenoise
)
