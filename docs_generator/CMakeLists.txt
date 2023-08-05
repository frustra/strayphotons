#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_executable(docs_generator
    main.cc
)

target_link_libraries(docs_generator
    ${PROJECT_CORE_LIB}
    ${PROJECT_GAME_LIB}
    ${PROJECT_SCRIPTS_LIB}
    cxxopts
)

#target_precompile_headers(docs_generator REUSE_FROM ${PROJECT_CORE_LIB})

target_include_directories(docs_generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(DOCS_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/docs/generated")

add_custom_command(
    COMMAND
        docs_generator "${DOCS_OUTPUT_PATH}"
    WORKING_DIRECTORY
        ${PROJECT_SOURCE_DIR}/bin
    OUTPUT
        "${DOCS_OUTPUT_PATH}"
    DEPENDS
        docs_generator
)

add_custom_target(component_docs ALL DEPENDS "${DOCS_OUTPUT_PATH}")
