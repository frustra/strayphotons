#
# Stray Photons - Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

## Define code generator executable
add_executable(${PROJECT_CODEGEN_LIB}-generator gen_main.cc)

target_include_directories(${PROJECT_CODEGEN_LIB}-generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_CODEGEN_LIB}-generator PRIVATE
    ${PROJECT_COMMON_LIB}
    ${PROJECT_CORE_LIB}
)

target_compile_definitions(${PROJECT_CODEGEN_LIB}-generator PRIVATE
    SP_EXECUTING_CODEGEN
)

## Invoke code generator
add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/include/strayphotons/components.h
        ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
    COMMAND
        ${PROJECT_CODEGEN_LIB}-generator
            ${CMAKE_CURRENT_BINARY_DIR}/include/strayphotons/components.h
            ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
    DEPENDS ${PROJECT_CODEGEN_LIB}-generator
)

add_custom_target(${PROJECT_CODEGEN_LIB}-output
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/include/strayphotons/components.h
        ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
)

add_library(${PROJECT_CODEGEN_LIB} OBJECT
    ${CMAKE_CURRENT_BINARY_DIR}/components_gen.cc
)

target_link_libraries(${PROJECT_CODEGEN_LIB} PRIVATE
    ${PROJECT_COMMON_LIB}
    ${PROJECT_CORE_LIB}
)

target_include_directories(${PROJECT_CODEGEN_LIB}
    PRIVATE
        ${PROJECT_ROOT_DIR}/include
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/include
)
add_dependencies(${PROJECT_CODEGEN_LIB} ${PROJECT_CODEGEN_LIB}-output)

target_compile_definitions(${PROJECT_CODEGEN_LIB} PRIVATE SP_SHARED_INTERNAL TECS_SHARED_INTERNAL)

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
        ${PROJECT_ROOT_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
    COMPILE_DEFINITIONS
        SP_SHARED_INTERNAL
        TECS_SHARED_INTERNAL
        VULKAN_HPP_STORAGE_SHARED
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
)

add_dependencies(${PROJECT_ECS_C_ABI_LIB} ${PROJECT_CODEGEN_LIB}-output)
