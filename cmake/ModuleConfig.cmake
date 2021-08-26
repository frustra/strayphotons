function(add_module_configuration)
    cmake_parse_arguments(
        "ARG"                       # Prefix
        ""                          # Flags
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

    set(SP_MODULE_CONFIG_LIST ${SP_MODULE_CONFIG_LIST} ${ARG_NAME} PARENT_SCOPE)
endfunction()

function(add_module_sources)
    foreach(module_config ${SP_MODULE_CONFIG_LIST})
        target_sources(${module_config} PRIVATE ${ARGV})
    endforeach()
endfunction()
