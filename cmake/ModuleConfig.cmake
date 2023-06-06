#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

function(add_module_configuration)
    cmake_parse_arguments(
        "ARG"                       # Prefix
        "EXPORT_COMPILE_COMMANDS"   # Flags
        "NAME"                      # One-value options
        "LINK_LIBRARIES"            # Multi-value options
        ${ARGV}                     # Args to parse
    )

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown arguments ${ARG_UNPARSED_ARGUMENTS} passed to add_module_configuration")
    endif()

    if(ARG_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "All arguments are required for add_module_configuration. Missing: ${ARG_KEYWORDS_MISSING_VALUES}")
    endif()

    # TODO: Commented until package release saves a log file
    # if(SP_PACKAGE_RELEASE AND WIN32)
    #     add_executable(${ARG_NAME} WIN32)
    # else()
        add_executable(${ARG_NAME})
    # endif()

    target_link_libraries(${ARG_NAME} PRIVATE
        ${PROJECT_COMMON_EXE}
        ${ARG_LINK_LIBRARIES}
    )

    if(ARG_EXPORT_COMPILE_COMMANDS)
        set_target_properties(${ARG_NAME} PROPERTIES EXPORT_COMPILE_COMMANDS 1)
    else()
        set_target_properties(${ARG_NAME} PROPERTIES EXPORT_COMPILE_COMMANDS 0)
    endif()

    set(SP_MODULE_CONFIG_LIST ${SP_MODULE_CONFIG_LIST} ${ARG_NAME} PARENT_SCOPE)
endfunction()

function(add_module_sources)
    foreach(module_config ${SP_MODULE_CONFIG_LIST})
        target_sources(${module_config} PRIVATE ${ARGV})
    endforeach()
endfunction()
