#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_SDK_LIB}-cpp STATIC)

target_include_directories(${PROJECT_SDK_LIB}-cpp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_SDK_LIB}-cpp PUBLIC
    glm
    magic_enum
    robin_hood
    Tecs
)

target_link_libraries(${PROJECT_SDK_LIB}-cpp PUBLIC ${PROJECT_SDK_LIB})

add_subdirectory(src)
