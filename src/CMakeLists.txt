#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(PROJECT_CORE_LIB ${PROJECT_BASE_NAME}-core CACHE INTERNAL "Library containing core components such as the ECS and asset loading")

set(PROJECT_GAME_LIB ${PROJECT_BASE_NAME}-game CACHE INTERNAL "Library implementing game engine ECS systems")
set(PROJECT_GAME_TEST_LIB ${PROJECT_BASE_NAME}-game-test CACHE INTERNAL "Interface library to enable CLI test mode")

set(PROJECT_GRAPHICS_CORE_LIB ${PROJECT_BASE_NAME}-graphics-core CACHE INTERNAL "Library containing context-independent graphics code")
set(PROJECT_GRAPHICS_VULKAN_CORE_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-core CACHE INTERNAL "Library containing a Vulkan-based graphics implementation")
set(PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-headless CACHE INTERNAL "Library for Vulkan-based rendering without a display")
set(PROJECT_GRAPHICS_VULKAN_XR_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-xr CACHE INTERNAL "Library for Vulkan-based rendering through an XR display")

#set(PROJECT_INPUT_LIB ${PROJECT_BASE_NAME}-input-winit CACHE INTERNAL "Library implementing user input support using winit")

set(PROJECT_PHYSICS_PHYSX_LIB ${PROJECT_BASE_NAME}-physics-physx CACHE INTERNAL "Library implementing physics using PhysX")
set(PROJECT_PHYSICS_COOKING_LIB ${PROJECT_BASE_NAME}-physics-cooking CACHE INTERNAL "Library implementing convex hull generation")

set(PROJECT_AUDIO_LIB ${PROJECT_BASE_NAME}-audio CACHE INTERNAL "Library implementing audio using libsoundio and Resonance Audio")

set(PROJECT_XR_CORE_LIB ${PROJECT_BASE_NAME}-xr-core CACHE INTERNAL "Library containing generic XR/VR support")
set(PROJECT_XR_OPENVR_LIB ${PROJECT_BASE_NAME}-xr-openvr CACHE INTERNAL "Library implementing XR support using OpenVR")

set(PROJECT_SCRIPTS_LIB ${PROJECT_BASE_NAME}-scripts CACHE INTERNAL "Library containing internal scripts")

include(${PROJECT_SOURCE_DIR}/cmake/ModuleConfig.cmake)

add_library(sp SHARED
    main/Game.cc
    graphics/GraphicsManager.cc
)

target_include_directories(sp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/main
)

target_link_libraries(sp PRIVATE
    sp-rs
    ${PROJECT_GAME_LIB}
    #${PROJECT_INPUT_LIB}
    ${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB}
    ${PROJECT_PHYSICS_PHYSX_LIB}
    ${PROJECT_SCRIPTS_LIB}
    cxxopts
)

if(WIN32)
    target_link_libraries(sp INTERFACE winmm)
endif()

# Vulkan desktop build
add_module_configuration(
    NAME sp-vk
    EXPORT_COMPILE_COMMANDS
    LINK_LIBRARIES
        ${PROJECT_GAME_LIB}
        #${PROJECT_INPUT_LIB}
        ${PROJECT_GRAPHICS_VULKAN_XR_LIB}
        ${PROJECT_XR_OPENVR_LIB}
        "$<IF:$<BOOL:${WIN32}>,${PROJECT_AUDIO_LIB},>"
        ${PROJECT_PHYSICS_PHYSX_LIB}
        ${PROJECT_SCRIPTS_LIB}
)

# Headless executable used for CI testing purposes
add_module_configuration(
    NAME sp-test
    LINK_LIBRARIES
        ${PROJECT_GAME_TEST_LIB}
        ${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB}
        #${PROJECT_INPUT_LIB}
        ${PROJECT_PHYSICS_PHYSX_LIB}
        ${PROJECT_SCRIPTS_LIB}
)

add_subdirectory(core)

#target_precompile_headers(sp-vk REUSE_FROM ${PROJECT_CORE_LIB})
#target_precompile_headers(sp-test REUSE_FROM ${PROJECT_CORE_LIB})


if(WIN32)
    add_subdirectory(audio)
endif()

add_subdirectory(game)
add_subdirectory(graphics)
#add_subdirectory(input)
add_subdirectory(main)
add_subdirectory(physx)
add_subdirectory(xr)
add_subdirectory(scripts)
