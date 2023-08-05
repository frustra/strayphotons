#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(${PROJECT_GAME_LIB} STATIC)
add_library(${PROJECT_GAME_TEST_LIB} INTERFACE)

target_include_directories(${PROJECT_GAME_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_GAME_LIB} PUBLIC
    ${PROJECT_CORE_LIB}
)

target_link_libraries(${PROJECT_GAME_TEST_LIB} INTERFACE
    ${PROJECT_GAME_LIB}
)

target_compile_definitions(${PROJECT_GAME_TEST_LIB} INTERFACE SP_TEST_MODE)

#target_precompile_headers(${PROJECT_GAME_LIB} REUSE_FROM ${PROJECT_CORE_LIB})

add_subdirectory(console)
add_subdirectory(editor)
add_subdirectory(game)

add_module_sources(
    DebugCFuncs.cc
)
