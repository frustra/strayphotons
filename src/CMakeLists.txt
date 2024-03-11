#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(PROJECT_GRAPHICS_VULKAN_GLFW_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-glfw CACHE INTERNAL "Library for Vulkan-based rendering to a display via GLFW")
set(PROJECT_GRAPHICS_VULKAN_GLFW_HEADLESS_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-glfw-headless CACHE INTERNAL "Library for Vulkan-based rendering without a display via GLFW")
set(PROJECT_GRAPHICS_VULKAN_GLFW_XR_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-glfw-xr CACHE INTERNAL "Library for Vulkan-based rendering through an XR display")
set(PROJECT_GRAPHICS_VULKAN_WINIT_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-winit CACHE INTERNAL "Library for Vulkan-based rendering to a display via Winit")

set(PROJECT_MAIN_WINIT_LIB ${PROJECT_BASE_NAME}-main-winit CACHE INTERNAL "Library implementing window creation using Winit")

set(PROJECT_AUDIO_LIB ${PROJECT_BASE_NAME}-audio CACHE INTERNAL "Library implementing audio using libsoundio and Resonance Audio")

set(PROJECT_XR_CORE_LIB ${PROJECT_BASE_NAME}-xr-core CACHE INTERNAL "Library containing generic XR/VR support")
set(PROJECT_XR_OPENVR_LIB ${PROJECT_BASE_NAME}-xr-openvr CACHE INTERNAL "Library implementing XR support using OpenVR")

include(${PROJECT_SOURCE_DIR}/cmake/ModuleConfig.cmake)

# Vulkan desktop build
add_module_configuration(
    NAME sp-vk
    EXPORT_COMPILE_COMMANDS
    LINK_LIBRARIES
        ${PROJECT_LIB_NAME}
        ${PROJECT_COMMON_LIB}
        # ${PROJECT_GRAPHICS_VULKAN_GLFW_XR_LIB}
        # "$<IF:$<BOOL:${WIN32}>,${PROJECT_AUDIO_LIB},>"
        # ${PROJECT_XR_OPENVR_LIB}
        glfw
)

if(SP_BUILD_RUST)
    # Winit desktop build (dynamic linking)
    add_executable(sp-winit
        main/winit/MainWinit.cc
        game/DebugCFuncs.cc
        xr/xr/XrManager.cc
        xr/xr/openvr/vulkan/VkSubmit.cc
    )
    target_link_libraries(sp-winit PRIVATE
        ${PROJECT_LIB_NAME}
        ${PROJECT_MAIN_WINIT_LIB}
        ${PROJECT_GRAPHICS_VULKAN_WINIT_LIB}
        "$<IF:$<BOOL:${WIN32}>,${PROJECT_AUDIO_LIB},>"
        ${PROJECT_XR_OPENVR_LIB}
    )
endif()

# Headless executable used for CI testing purposes
add_module_configuration(
    NAME sp-test
    LINK_LIBRARIES
        ${PROJECT_LIB_NAME}
        # ${PROJECT_GRAPHICS_VULKAN_GLFW_HEADLESS_LIB}
        glfw
)

if(WIN32)
    add_subdirectory(audio)
endif()

add_subdirectory(main)
add_subdirectory(xr)
