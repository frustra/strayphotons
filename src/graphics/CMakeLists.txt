#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_GRAPHICS_CORE_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} STATIC)

target_include_directories(${PROJECT_GRAPHICS_CORE_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_ROOT_DIR}/include
)

target_include_directories(${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_ROOT_DIR}/include
)

target_link_libraries(${PROJECT_GRAPHICS_CORE_LIB}
    PRIVATE
        ${PROJECT_COMMON_LIB}
        ${PROJECT_CORE_LIB}
        ${PROJECT_GAME_LIB}
        cxxopts
        ImGui
)

target_link_libraries(${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    PUBLIC
        ${PROJECT_GRAPHICS_CORE_LIB}
        Vulkan::Vulkan
        VulkanMemoryAllocator
    PRIVATE
        ${PROJECT_COMMON_LIB}
        ${PROJECT_CORE_LIB}
        ${PROJECT_GAME_LIB}
        SPIRV-Reflect
        fpng
)

target_compile_definitions(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT
    # TRACY_ENABLE_GRAPHICS
)
target_compile_definitions(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_VK
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
)

# target_precompile_headers(${PROJECT_GRAPHICS_CORE_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
# target_precompile_headers(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(graphics)
