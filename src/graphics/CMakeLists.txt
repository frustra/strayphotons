#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

find_package(Vulkan REQUIRED)

add_library(${PROJECT_GRAPHICS_CORE_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_GLFW_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_WINIT_LIB} STATIC)
add_library(${PROJECT_GRAPHICS_VULKAN_XR_LIB} STATIC)

target_include_directories(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
    ImGui
)

target_link_libraries(${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    PUBLIC
        ${PROJECT_GRAPHICS_CORE_LIB}
        Vulkan::Vulkan
        VulkanMemoryAllocator
    PRIVATE
        SPIRV-Reflect
        fpng
)
target_link_libraries(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} PUBLIC
    ${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    glfw-egl # Prevents X11 context creation
)
target_link_libraries(${PROJECT_GRAPHICS_VULKAN_GLFW_LIB} PUBLIC
    ${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    glfw
)
target_link_libraries(${PROJECT_GRAPHICS_VULKAN_WINIT_LIB} PUBLIC
    ${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    ${PROJECT_INPUT_WINIT_LIB}
    sp-rs-window
)
target_link_libraries(${PROJECT_GRAPHICS_VULKAN_XR_LIB} PUBLIC
    ${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    ${PROJECT_XR_CORE_LIB}
)

target_compile_definitions(${PROJECT_GRAPHICS_CORE_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT
    # TRACY_ENABLE_GRAPHICS
)
target_compile_definitions(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_VK
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
)
target_compile_definitions(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_HEADLESS
)
target_compile_definitions(${PROJECT_GRAPHICS_VULKAN_GLFW_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_GLFW
)
target_compile_definitions(${PROJECT_GRAPHICS_VULKAN_WINIT_LIB} PUBLIC
    SP_GRAPHICS_SUPPORT_WINIT
)

target_precompile_headers(${PROJECT_GRAPHICS_CORE_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_GRAPHICS_VULKAN_GLFW_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_GRAPHICS_VULKAN_WINIT_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(${PROJECT_GRAPHICS_VULKAN_XR_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(graphics)

add_module_sources(
    GraphicsManager.cc
)
