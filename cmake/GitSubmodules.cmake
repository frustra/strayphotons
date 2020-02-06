function(sync_submodules)
    cmake_parse_arguments(
        "ARG"                       # Prefix
        ""                          # Flags
        "NAME;GITMODULES;PRODUCES"  # One-value options
        "NEEDED_BY"                 # Multi-value options
        ${ARGV}                     # Args to parse
    )

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown arguments ${ARG_UNPARSED_ARGUMENTS} passed to sync_submodules")
    endif()

    if(ARG_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "All arguments are required for sync_submodules. Missing: ${ARG_KEYWORDS_MISSING_VALUES}")
    endif()

    ## Test if submodule needs to be initialized
    if (NOT IS_DIRECTORY ${ARG_PRODUCES})
        message(STATUS "Submodule ${ARG_NAME} does not appear to be initialized!")
    endif()

    ## Make "git submodule sync --recursive" + "git submodule update --init --recursive" depend on changes to the .gitmodules file
    ## Make "git submodule update --init --recursive" happen if submodules are not initialized

    if(NOT EXISTS ${ARG_GITMODULES})
        message(FATAL_ERROR "${ARG_GITMODULES} does not point to a file.")
    endif()

    ## Add a command to run "git submodule update --init --recursive -- <submodule path>"

    ## Add a command to run "git submodule sync --recursive -- <submodule path>" if it detects
    ## a change to the .gitmodules file.
    ## This will always run during the first build, and will then automatically run whenever the 
    ## .gitmodules file is changed.
    add_custom_command(
        DEPENDS
            ${ARG_GITMODULES}
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}-submodule-sync
        COMMAND
            cd ${PROJECT_SOURCE_DIR}
        COMMAND
            git submodule sync --recursive -- ${ARG_PRODUCES}
        COMMAND
            ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}-submodule-sync
    )

    add_custom_target(
        ${ARG_NAME}-submodule-sync-target
        DEPENDS 
            ${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}-submodule-sync
    )

    foreach(_target ${ARG_NEEDED_BY})
        add_dependencies(${_target} ${ARG_NAME}-submodule-sync-target)
    endforeach()

endfunction()