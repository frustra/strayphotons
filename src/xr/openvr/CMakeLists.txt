#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

target_sources(${PROJECT_XR_OPENVR_LIB} PRIVATE
    EventHandler.cc
    InputBindings.cc
    OpenVrSystem.cc
)

set(_files_to_copy
    actions.json
    sp_bindings_oculus_touch.json
    sp_bindings_knuckles.json
    sp_bindings_vive_controller.json
)

foreach(_file ${_files_to_copy})
    # Ensure openvr ends up in the runtime output directory
    add_custom_command(
        OUTPUT
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_file}
        DEPENDS
            ${CMAKE_CURRENT_LIST_DIR}/${_file}
        COMMAND
            ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/${_file} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_file}
    )

    add_custom_target(
        ${_file}-runtime-copy
        DEPENDS
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_file}
            ${CMAKE_CURRENT_LIST_DIR}/${_file}
    )

    add_dependencies(${PROJECT_XR_OPENVR_LIB} ${_file}-runtime-copy)

endforeach()
