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

target_precompile_headers(docs_generator REUSE_FROM ${PROJECT_CORE_LIB})

target_include_directories(docs_generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(DOCS_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/docs/generated")
add_custom_target(component_docs ALL)

function(generate_docs_pages)
    math(EXPR page_count "${ARGC} / 2")
    foreach(i RANGE 1 ${page_count})
        math(EXPR arg_index "${i} * 2 - 2")
        list(GET ARGV ${arg_index} page_name)
        math(EXPR arg_index "${arg_index} + 1")
        list(GET ARGV ${arg_index} page_path)

        add_custom_command(
            COMMAND
                docs_generator ${page_name} "${DOCS_OUTPUT_PATH}/${page_path}"
            WORKING_DIRECTORY
                ${PROJECT_SOURCE_DIR}/bin
            OUTPUT
                "${DOCS_OUTPUT_PATH}/${page_path}"
            DEPENDS
                docs_generator
        )
        
        add_custom_target("component_docs_${page_name}" DEPENDS "${DOCS_OUTPUT_PATH}/${page_path}")
        add_dependencies(component_docs "component_docs_${page_name}")
    endforeach()
endfunction()

generate_docs_pages(
    general "General_Components.md"
    rendering "Rendering_Components.md"
    physics "Physics_Components.md"
    other "Other_Components.md"
    schema "scene.schema.json"
)
