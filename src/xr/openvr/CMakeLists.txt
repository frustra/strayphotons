if(NOT XRSYSTEM_OPENVR)
    return()
endif()

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        OpenVrModel.cc
        OpenVrSystem.cc
        OpenVrTrackingCompositor.cc
        OpenVrInputSources.cc
)