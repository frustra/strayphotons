#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(PROJECT_COMMON_LIB ${PROJECT_BASE_NAME}-common CACHE INTERNAL "Library containing common unitility functions")
set(PROJECT_CORE_LIB ${PROJECT_BASE_NAME}-core CACHE INTERNAL "Library containing core components such as the ECS and asset loading")
set(PROJECT_GAME_LIB ${PROJECT_BASE_NAME}-game CACHE INTERNAL "Library implementing game engine ECS systems")

set(PROJECT_GRAPHICS_CORE_LIB ${PROJECT_BASE_NAME}-graphics-core CACHE INTERNAL "Library containing context-independent graphics code")
set(PROJECT_GRAPHICS_VULKAN_CORE_LIB ${PROJECT_BASE_NAME}-graphics-vulkan-core CACHE INTERNAL "Library containing a Vulkan-based graphics implementation")

set(PROJECT_AUDIO_LIB ${PROJECT_BASE_NAME}-audio CACHE INTERNAL "Library implementing audio using libsoundio and Resonance Audio")

set(PROJECT_PHYSICS_PHYSX_LIB ${PROJECT_BASE_NAME}-physics-physx CACHE INTERNAL "Library implementing physics using PhysX")
set(PROJECT_PHYSICS_COOKING_LIB ${PROJECT_BASE_NAME}-physics-cooking CACHE INTERNAL "Library implementing convex hull generation")

set(PROJECT_SCRIPTS_LIB ${PROJECT_BASE_NAME}-scripts CACHE INTERNAL "Library containing built-in scripts")


add_library(${PROJECT_LIB_NAME} SHARED)

target_include_directories(${PROJECT_LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_LIB_NAME} PRIVATE
    ${PROJECT_COMMON_LIB}
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
    ${PROJECT_PHYSICS_PHYSX_LIB}
    ${PROJECT_GRAPHICS_VULKAN_CORE_LIB}
    "$<IF:$<BOOL:${WIN32}>,${PROJECT_AUDIO_LIB},>"
    "$<IF:$<BOOL:SP_BUILD_RUST>,sp-rs-wasm,>"
    ${PROJECT_SCRIPTS_LIB}
)

target_compile_definitions(${PROJECT_LIB_NAME}
    PRIVATE
        SP_SHARED_INTERNAL
    INTERFACE
        SP_SHARED_BUILD
)

add_subdirectory(src)
