#
# Stray Photons - Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

## Define code generator executable
add_executable(${PROJECT_EXPORTS_LIB}-generator
    gen_main.cc
)

target_include_directories(${PROJECT_EXPORTS_LIB}-generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_EXPORTS_LIB}-generator PRIVATE
    ${PROJECT_COMMON_LIB}
    ${PROJECT_CORE_LIB}
)

target_compile_definitions(${PROJECT_EXPORTS_LIB}-generator PRIVATE
    SP_EXECUTING_CODEGEN
)

set_target_properties(${PROJECT_EXPORTS_LIB}-generator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

## Invoke code generator
add_custom_command(
    OUTPUT
        ${PROJECT_ROOT_DIR}/sdk/include/strayphotons/components.h
        ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
    COMMAND
        ${PROJECT_EXPORTS_LIB}-generator
            ${PROJECT_ROOT_DIR}/sdk/include/strayphotons/components.h
            ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
    DEPENDS ${PROJECT_EXPORTS_LIB}-generator
)

add_custom_target(${PROJECT_EXPORTS_LIB}-output
    DEPENDS
        ${PROJECT_ROOT_DIR}/sdk/include/strayphotons/components.h
        ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
)

TecsGenerateCHeaders(
    TARGET_NAME ${PROJECT_EXPORTS_LIB} OBJECT_TARGET
    ECS_INCLUDE_PATH "ecs/EcsImpl.hh"
    ECS_C_INCLUDE_PATH "strayphotons/components.h"
    ECS_NAME ecs::ECS
    COMPONENT_TYPE_PREFIX "sp_ecs_"
    LINK_LIBRARIES
        ${PROJECT_COMMON_LIB}
        ${PROJECT_CORE_LIB}
    COMPILE_DEFINITIONS
        VULKAN_HPP_STORAGE_SHARED
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
        SP_SHARED_INTERNAL
)

add_dependencies(${PROJECT_EXPORTS_LIB} ${PROJECT_EXPORTS_LIB}-output)
