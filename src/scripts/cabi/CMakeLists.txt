#
# Stray Photons - Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(camera_view SHARED camera_view.c)

target_link_libraries(camera_view PRIVATE ${PROJECT_LIB_NAME} cglm)

if(WIN32)
    add_custom_command(TARGET camera_view PRE_LINK
        COMMAND
            ./extra/remove_in_use.py camera_view dll pdb
        WORKING_DIRECTORY ${PROJECT_ROOT_DIR}
    )
endif()
