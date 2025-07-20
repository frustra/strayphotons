#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_CORE_LIB} STATIC)

target_include_directories(${PROJECT_CORE_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_ROOT_DIR}/include
)

target_link_libraries(${PROJECT_CORE_LIB}
    PUBLIC
        ${PROJECT_COMMON_LIB}
        Tecs
        glm
        picojson
        magic_enum
        robin_hood
        tinygltf
    PRIVATE
        microtar
        murmurhash
)

if(WIN32)
    target_compile_definitions(${PROJECT_CORE_LIB} PUBLIC
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
        NOMINMAX
        _USE_MATH_DEFINES
    )

    # fatal error C1128: number of sections exceeded object file format limit: compile with /bigobj
    target_compile_options(${PROJECT_CORE_LIB} PUBLIC /bigobj)
else()
    target_link_libraries(${PROJECT_CORE_LIB} PRIVATE
        linenoise
    )
endif()

# _DEBUG/NDEBUG Used by PhysX, must be defined everywhere to enable precompiled headers
# NDEBUG Used by vulkan.hpp DispatchLoaderBase
if (SP_DEBUG)
    target_compile_definitions(${PROJECT_CORE_LIB} PUBLIC _DEBUG)
else()
    target_compile_definitions(${PROJECT_CORE_LIB} PUBLIC NDEBUG)
endif()

add_subdirectory(assets)
add_subdirectory(console)
add_subdirectory(ecs)
add_subdirectory(game)
