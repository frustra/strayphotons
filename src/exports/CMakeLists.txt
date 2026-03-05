#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_subdirectory(codegen)

target_sources(${PROJECT_LIB_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/codegen/components_gen.cc
    Console.cc
    Ecs.cc
    Game.cc
    Graphics.cc
    Input.cc
    Logging.cc
    Threads.cc
)

target_include_directories(${PROJECT_LIB_NAME}
    PUBLIC
        ${PROJECT_ROOT_DIR}/sdk/include
)
add_dependencies(${PROJECT_LIB_NAME} ${PROJECT_EXPORTS_LIB}-output)
