#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_executable(hull_compiler
    main.cc
)

target_link_libraries(hull_compiler
    ${PROJECT_CORE_LIB}
    ${PROJECT_PHYSICS_COOKING_LIB}
    vhacd
    cxxopts
)

#target_precompile_headers(hull_compiler REUSE_FROM ${PROJECT_CORE_LIB})

target_include_directories(hull_compiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
