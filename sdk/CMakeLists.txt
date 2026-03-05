#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

project(sp-sdk VERSION 0.1)

add_library(${PROJECT_SDK_LIB} STATIC)

target_include_directories(${PROJECT_SDK_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_SDK_LIB} PUBLIC
    glm
    magic_enum
    robin_hood
)

target_link_libraries(${PROJECT_SDK_LIB} PUBLIC ${PROJECT_LIB_NAME})

add_subdirectory(src)
