#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

## Define code generator executable
add_executable(sdk_generator gen_main.cc)

target_include_directories(sdk_generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sdk_generator PRIVATE
    ${PROJECT_COMMON_LIB}
    ${PROJECT_CORE_LIB}
)

target_compile_definitions(sdk_generator PRIVATE
    SP_EXECUTING_CODEGEN
    SP_SHARED_INTERNAL
    TECS_SHARED_INTERNAL
)

set_target_properties(sdk_generator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

## Invoke code generator
add_custom_command(
    OUTPUT
        ${PROJECT_SOURCE_DIR}/sdk/include/strayphotons/components.h
        ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
    COMMAND
        sdk_generator
            ${PROJECT_SOURCE_DIR}/sdk/include/strayphotons/components.h
            ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
    DEPENDS sdk_generator
)

add_custom_target(sdk_generated_output
    DEPENDS
        ${PROJECT_SOURCE_DIR}/sdk/include/strayphotons/components.h
        ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
)

add_library(${PROJECT_SDK_EXPORTS_LIB} OBJECT
    ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
)

target_link_libraries(${PROJECT_SDK_EXPORTS_LIB} PRIVATE
    ${PROJECT_COMMON_LIB}
    ${PROJECT_CORE_LIB}
)

target_include_directories(${PROJECT_SDK_EXPORTS_LIB}
    PUBLIC
        ${PROJECT_ROOT_DIR}/sdk/include
)
add_dependencies(${PROJECT_SDK_EXPORTS_LIB} sdk_generated_output)

target_compile_definitions(${PROJECT_SDK_EXPORTS_LIB} PRIVATE SP_SHARED_INTERNAL TECS_SHARED_INTERNAL)

TecsGenerateCHeaders(
    TARGET_NAME ${PROJECT_ECS_C_ABI_LIB} OBJECT_TARGET
    ECS_INCLUDE_PATH "ecs/EcsImpl.hh"
    ECS_C_INCLUDE_PATH "strayphotons/components.h"
    ECS_NAME ecs::ECS
    COMPONENT_TYPE_PREFIX "sp_ecs_"
    LINK_LIBRARIES
        ${PROJECT_COMMON_LIB}
        ${PROJECT_CORE_LIB}
    INCLUDE_DIRECTORIES
        ${PROJECT_ROOT_DIR}/sdk/include
    COMPILE_DEFINITIONS
        SP_SHARED_INTERNAL
        TECS_SHARED_INTERNAL
        VULKAN_HPP_STORAGE_SHARED
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
)

add_dependencies(${PROJECT_ECS_C_ABI_LIB} sdk_generated_output)
