#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_COMMON_LIB} STATIC)

target_include_directories(${PROJECT_COMMON_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_ROOT_DIR}/include
)

target_link_libraries(${PROJECT_COMMON_LIB} PUBLIC
    glm
    magic_enum
    TracyClient
    robin_hood
    Tecs
)

add_subdirectory(common)
