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

add_library(hello_cpp SHARED hello_cpp.cpp)

target_link_libraries(hello_cpp PRIVATE ${PROJECT_LIB_NAME} ${PROJECT_COMMON_LIB} glm)

if(WIN32)
    target_compile_definitions(hello_cpp PUBLIC _CRT_SECURE_NO_WARNINGS)
    add_custom_command(TARGET hello_cpp PRE_LINK
        COMMAND
            ./extra/remove_in_use.py hello_cpp dll pdb
        WORKING_DIRECTORY ${PROJECT_ROOT_DIR}
    )
endif()
