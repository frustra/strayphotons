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
        ${PROJECT_ROOT_DIR}/shared/include
)

target_link_libraries(${PROJECT_COMMON_LIB} PUBLIC
    glm
    magic_enum
    TracyClient
    robin_hood
    Tecs
)

if(WIN32)
    target_compile_definitions(${PROJECT_COMMON_LIB} PUBLIC
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
        NOMINMAX
        _USE_MATH_DEFINES
    )
endif()

# _DEBUG/NDEBUG Used by PhysX, must be defined everywhere to enable precompiled headers
if (SP_DEBUG)
    target_compile_definitions(${PROJECT_COMMON_LIB} PUBLIC _DEBUG)
else()
    target_compile_definitions(${PROJECT_COMMON_LIB} PUBLIC NDEBUG)
endif()

add_subdirectory(common)
