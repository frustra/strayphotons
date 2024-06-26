#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_GAME_LIB} STATIC)

target_include_directories(${PROJECT_GAME_LIB}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_ROOT_DIR}/include
)

target_link_libraries(${PROJECT_GAME_LIB}
    PUBLIC
        cxxopts
    PRIVATE
        ${PROJECT_COMMON_LIB}
        ${PROJECT_CORE_LIB}
)

if(WIN32)
    # Include timeBeginPeriod() and timeEndPeriod() on windows
    target_link_libraries(${PROJECT_GAME_LIB} PUBLIC winmm)
endif()

target_precompile_headers(${PROJECT_GAME_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(console)
add_subdirectory(editor)
add_subdirectory(game)

target_sources(${PROJECT_LIB_NAME} PRIVATE
    DebugCFuncs.cc
    InitSystems.cc
)
