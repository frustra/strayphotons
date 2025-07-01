#
# Stray Photons - Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(SCRIPT_LIST
    camera_view
    game_scripts
    hello_world
    life
)

foreach(SCRIPT_NAME ${SCRIPT_LIST})
    add_library(${SCRIPT_NAME} SHARED ${SCRIPT_NAME}.c)

    target_link_libraries(${SCRIPT_NAME} PRIVATE ${PROJECT_LIB_NAME} cglm)

    if(WIN32)
        add_custom_command(TARGET ${SCRIPT_NAME} PRE_LINK
            COMMAND
                ./extra/remove_in_use.py ${SCRIPT_NAME} dll pdb
            WORKING_DIRECTORY ${PROJECT_ROOT_DIR}
        )
    endif()
endforeach()
