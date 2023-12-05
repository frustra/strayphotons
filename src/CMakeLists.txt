#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(PROJECT_CORE_LIB ${PROJECT_BASE_NAME}-core CACHE INTERNAL "Library containing core components such as the ECS and asset loading")

set(PROJECT_GAME_LIB ${PROJECT_BASE_NAME}-game CACHE INTERNAL "Library implementing game engine ECS systems")

set(PROJECT_GRAPHICS_CORE_LIB ${PROJECT_BASE_NAME}-graphics-core CACHE INTERNAL "Library containing context-independent graphics code")
set(PROJECT_GRAPHICS_VULKAN_CORE_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-core CACHE INTERNAL "Library containing a Vulkan-based graphics implementation")
set(PROJECT_GRAPHICS_VULKAN_GLFW_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-glfw CACHE INTERNAL "Library for Vulkan-based rendering to a display via GLFW")
set(PROJECT_GRAPHICS_VULKAN_GLFW_HEADLESS_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-glfw-headless CACHE INTERNAL "Library for Vulkan-based rendering without a display via GLFW")
set(PROJECT_GRAPHICS_VULKAN_GLFW_XR_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-glfw-xr CACHE INTERNAL "Library for Vulkan-based rendering through an XR display")
set(PROJECT_GRAPHICS_VULKAN_WINIT_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-winit CACHE INTERNAL "Library for Vulkan-based rendering to a display via Winit")

set(PROJECT_MAIN_WINIT_LIB ${PROJECT_BASE_NAME}-main-winit CACHE INTERNAL "Library implementing window creation using Winit")

set(PROJECT_PHYSICS_PHYSX_LIB ${PROJECT_BASE_NAME}-physics-physx CACHE INTERNAL "Library implementing physics using PhysX")
set(PROJECT_PHYSICS_COOKING_LIB ${PROJECT_BASE_NAME}-physics-cooking CACHE INTERNAL "Library implementing convex hull generation")

set(PROJECT_AUDIO_LIB ${PROJECT_BASE_NAME}-audio CACHE INTERNAL "Library implementing audio using libsoundio and Resonance Audio")

set(PROJECT_XR_CORE_LIB ${PROJECT_BASE_NAME}-xr-core CACHE INTERNAL "Library containing generic XR/VR support")
set(PROJECT_XR_OPENVR_LIB ${PROJECT_BASE_NAME}-xr-openvr CACHE INTERNAL "Library implementing XR support using OpenVR")

set(PROJECT_SCRIPTS_LIB ${PROJECT_BASE_NAME}-scripts CACHE INTERNAL "Library containing internal scripts")

include(${PROJECT_SOURCE_DIR}/cmake/ModuleConfig.cmake)

add_library(sp SHARED
    main/StrayPhotons.cc
    main/Game.cc
)

target_compile_definitions(sp PUBLIC SP_SHARED_BUILD)
target_include_directories(sp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${PROJECT_ROOT_DIR}/include
)
target_link_libraries(sp PRIVATE
    ${PROJECT_GAME_LIB}
    ${PROJECT_GRAPHICS_CORE_LIB}
    "$<IF:$<BOOL:${WIN32}>,${PROJECT_AUDIO_LIB},>"
    ${PROJECT_PHYSICS_PHYSX_LIB}
    ${PROJECT_SCRIPTS_LIB}
    cxxopts
)

if(WIN32)
    target_link_libraries(sp PUBLIC winmm)
endif()

# Vulkan desktop build
add_module_configuration(
    NAME sp-vk
    EXPORT_COMPILE_COMMANDS
    LINK_LIBRARIES
        sp
        ${PROJECT_GAME_LIB}
        ${PROJECT_GRAPHICS_VULKAN_GLFW_XR_LIB}
        ${PROJECT_XR_OPENVR_LIB}
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
        sp
        ${PROJECT_MAIN_WINIT_LIB}
        ${PROJECT_GRAPHICS_VULKAN_WINIT_LIB}
        ${PROJECT_XR_OPENVR_LIB}
    )
endif()

# Headless executable used for CI testing purposes
add_module_configuration(
    NAME sp-test
    LINK_LIBRARIES
        sp
        ${PROJECT_GAME_LIB}
        ${PROJECT_GRAPHICS_VULKAN_GLFW_HEADLESS_LIB}
)

add_subdirectory(core)

target_precompile_headers(sp REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(sp-vk REUSE_FROM ${PROJECT_CORE_LIB})
target_precompile_headers(sp-test REUSE_FROM ${PROJECT_CORE_LIB})


if(WIN32)
    add_subdirectory(audio)
endif()

add_subdirectory(game)
add_subdirectory(graphics)
add_subdirectory(main)
add_subdirectory(physx)
add_subdirectory(xr)
add_subdirectory(scripts)
