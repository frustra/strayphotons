#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

if(SP_PACKAGE_RELEASE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
endif()
set(TRACY_ENABLE ${SP_ENABLE_TRACY} CACHE BOOL "" FORCE)
set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-unused-but-set-variable -Wno-array-bounds)
endif()

add_subdirectory(tracy)
