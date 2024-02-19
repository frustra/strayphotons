#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_PHYSICS_PHYSX_LIB} STATIC)
add_library(${PROJECT_PHYSICS_COOKING_LIB} STATIC)

target_include_directories(${PROJECT_PHYSICS_PHYSX_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(${PROJECT_PHYSICS_COOKING_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_PHYSICS_PHYSX_LIB}
    PUBLIC
        PhysX
    PRIVATE
        ${PROJECT_COMMON_LIB}
        ${PROJECT_CORE_LIB}
        ${PROJECT_GAME_LIB}
        ${PROJECT_PHYSICS_COOKING_LIB}
        PhysXCharacterKinematic
        PhysXPvdSDK
)
target_link_libraries(${PROJECT_PHYSICS_COOKING_LIB}
    PRIVATE
        ${PROJECT_COMMON_LIB}
        ${PROJECT_CORE_LIB}
        vhacd
        PhysXCooking
        PhysXExtensions
)

target_compile_definitions(${PROJECT_PHYSICS_PHYSX_LIB}
    PUBLIC
        SP_PHYSICS_SUPPORT_PHYSX
)

# target_precompile_headers(${PROJECT_PHYSICS_PHYSX_LIB} REUSE_FROM ${PROJECT_CORE_LIB})
# target_precompile_headers(${PROJECT_PHYSICS_COOKING_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(cooking)
add_subdirectory(physx)
