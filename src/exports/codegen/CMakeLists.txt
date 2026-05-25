#
# Stray Photons - Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

## Define code generator executable
add_executable(${PROJECT_EXPORTS_LIB}-components-codegen
    gen_main.cc
)

target_include_directories(${PROJECT_EXPORTS_LIB}-components-codegen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_EXPORTS_LIB}-components-codegen PRIVATE
    ${PROJECT_COMMON_LIB}
    ${PROJECT_CORE_LIB}
)

target_compile_definitions(${PROJECT_EXPORTS_LIB}-components-codegen PRIVATE
    SP_EXECUTING_CODEGEN
)

set_target_properties(${PROJECT_EXPORTS_LIB}-components-codegen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

## Invoke code generator
add_custom_command(
    OUTPUT
        ${PROJECT_ROOT_DIR}/sdk/include/strayphotons/components_gen.h
        ${CMAKE_CURRENT_BINARY_DIR}/include/components_gen_internal.hh
        ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
    COMMAND
        ${PROJECT_EXPORTS_LIB}-components-codegen
            ${PROJECT_ROOT_DIR}/sdk/include/strayphotons/components_gen.h
            ${CMAKE_CURRENT_BINARY_DIR}/include/components_gen_internal.hh
            ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
    DEPENDS ${PROJECT_EXPORTS_LIB}-components-codegen
)

add_custom_target(${PROJECT_EXPORTS_LIB}-output
    DEPENDS
        ${PROJECT_ROOT_DIR}/sdk/include/strayphotons/components_gen.h
        ${CMAKE_CURRENT_BINARY_DIR}/include/components_gen_internal.hh
        ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
)

TecsGenerateCHeaders(
    TARGET_NAME ${PROJECT_EXPORTS_LIB} OBJECT_TARGET
    HEADER_OUTPUT_DIR ${PROJECT_ROOT_DIR}/sdk/include/strayphotons
    ECS_INCLUDE_PATH "strayphotons/components_gen.h"
    ECS_IMPL_PATH "components_gen_internal.hh"
    ECS_NAME ecs::ECS
    COMPONENT_TYPE_PREFIX "sp_ecs_"
    LINK_LIBRARIES
        ${PROJECT_COMMON_LIB}
        ${PROJECT_CORE_LIB}
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${PROJECT_ROOT_DIR}/sdk/include
    COMPILE_DEFINITIONS
        VULKAN_HPP_STORAGE_SHARED
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
        SP_SHARED_INTERNAL
)

add_dependencies(${PROJECT_EXPORTS_LIB}-codegen ${PROJECT_EXPORTS_LIB}-output)
add_dependencies(${PROJECT_EXPORTS_LIB} ${PROJECT_EXPORTS_LIB}-output)
