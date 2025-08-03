#
# Stray Photons - Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(C_SCRIPT_LIST
    camera_view
    game_scripts
    hello_world
    life
)

set(CPP_SCRIPT_LIST
    hello_cpp
)

# Put plugin library outputs into a subfolder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR}/plugins) # Windows DLLs
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR}/plugins) # Linux SOs

# For multi-config builds (e.g. msvc)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJECT_OUTPUT_DIR}/plugins)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJECT_OUTPUT_DIR}/plugins)
endforeach()

foreach(SCRIPT_NAME ${C_SCRIPT_LIST})
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

foreach(SCRIPT_NAME ${CPP_SCRIPT_LIST})
    add_library(${SCRIPT_NAME} SHARED ${SCRIPT_NAME}.cpp)

    target_link_libraries(${SCRIPT_NAME} PRIVATE ${PROJECT_LIB_NAME} ${PROJECT_COMMON_LIB} glm)

    if(WIN32)
        target_compile_definitions(${SCRIPT_NAME} PUBLIC _CRT_SECURE_NO_WARNINGS)
        add_custom_command(TARGET ${SCRIPT_NAME} PRE_LINK
            COMMAND
                ./extra/remove_in_use.py ${SCRIPT_NAME} dll pdb
            WORKING_DIRECTORY ${PROJECT_ROOT_DIR}
        )
    endif()
endforeach()
