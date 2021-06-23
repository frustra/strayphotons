if(NOT XRSYSTEM_OPENVR)
    return()
endif()

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        OpenVrModel.cc
        OpenVrSystem.cc
        OpenVrTrackingCompositor.cc
        OpenVrAction.cc
)

# Need to link against openvr to support this XR backend
target_link_libraries(
    ${PROJECT_LIB}
    PUBLIC
        openvr
)

set(_files_to_copy
    actions.json
    sp_bindings_oculus_touch.json
    sp_bindings_knuckles.json
    sp_bindings_vive_controller.json
)

foreach(_file ${_files_to_copy})
    install(
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/${_file}
        DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        COMPONENT
            ${PROJECT_INSTALL}
    )

    # Ensure openvr ends up in the runtime output directory
    add_custom_command(
        OUTPUT
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_file}
        COMMAND   
            ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/${_file} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_file}
    )

    add_custom_target(
        ${_file}-runtime-copy
        DEPENDS
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_file}
            ${CMAKE_CURRENT_LIST_DIR}/${_file}
    )

    add_dependencies(${PROJECT_LIB} ${_file}-runtime-copy)

endforeach()
