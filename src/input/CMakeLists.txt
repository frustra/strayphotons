#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_INPUT_GLFW_LIB} STATIC)
add_library(${PROJECT_INPUT_WINIT_LIB} STATIC)

target_include_directories(${PROJECT_INPUT_GLFW_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(${PROJECT_INPUT_WINIT_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_INPUT_GLFW_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    glfw
)

target_link_libraries(${PROJECT_INPUT_WINIT_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    sp-rs
)

target_compile_definitions(${PROJECT_INPUT_GLFW_LIB} PUBLIC
    SP_INPUT_SUPPORT_GLFW
)

target_compile_definitions(${PROJECT_INPUT_WINIT_LIB} PUBLIC
    SP_INPUT_SUPPORT_WINIT
)

target_precompile_headers(${PROJECT_INPUT_GLFW_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_INPUT_WINIT_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(input)
