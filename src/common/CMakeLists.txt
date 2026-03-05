#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_COMMON_LIB} STATIC
    ${PROJECT_ROOT_DIR}/sdk/src/DispatchQueue.cc
    ${PROJECT_ROOT_DIR}/sdk/src/LockFreeMutex.cc
    ${PROJECT_ROOT_DIR}/sdk/src/Utility.cc
)

target_include_directories(${PROJECT_COMMON_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_ROOT_DIR}/sdk/include
)

target_link_libraries(${PROJECT_COMMON_LIB}
    PUBLIC
        glm
        magic_enum
        TracyClient
        robin_hood
        Tecs
)

target_compile_definitions(${PROJECT_COMMON_LIB} PUBLIC
    SP_SHARED_INTERNAL
    TECS_SHARED_INTERNAL
)

if(SP_DEBUG)
    target_compile_definitions(${PROJECT_COMMON_LIB} PUBLIC TRACY_ENABLE_DEBUG)
endif()

add_subdirectory(common)
