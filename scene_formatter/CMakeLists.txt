#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_executable(scene_formatter
    main.cc
)

target_link_libraries(scene_formatter
    ${PROJECT_COMMON_LIB}
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
    ${PROJECT_SCRIPTS_LIB}
    cxxopts
)

# target_precompile_headers(scene_formatter REUSE_FROM ${PROJECT_CORE_LIB})

target_include_directories(scene_formatter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
