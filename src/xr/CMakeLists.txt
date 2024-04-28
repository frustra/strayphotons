#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_XR_OPENVR_LIB} STATIC)

target_include_directories(${PROJECT_XR_OPENVR_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_XR_OPENVR_LIB}
    PUBLIC
        openvr
    PRIVATE
        ${PROJECT_COMMON_LIB}
        ${PROJECT_CORE_LIB}
        ${PROJECT_GAME_LIB}
        ${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
)

target_compile_definitions(${PROJECT_XR_OPENVR_LIB} PUBLIC
    SP_XR_SUPPORT
    SP_XR_SUPPORT_OPENVR
)

target_precompile_headers(${PROJECT_XR_OPENVR_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(openvr)
