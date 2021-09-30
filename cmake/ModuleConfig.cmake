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

    if(SP_PACKAGE_RELEASE)
        add_executable(${ARG_NAME} WIN32)
    else()
        add_executable(${ARG_NAME})
    endif()

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
    foreach(module_config_lib ${SP_MODULE_CONFIG_LIST})
        target_sources(${module_config_lib} PRIVATE ${ARGV})
    endforeach()
endfunction()
