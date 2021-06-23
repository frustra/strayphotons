add_subdirectory(openvr)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        XrCompositor.cc
        XrManager.cc
        XrSystemFactory.cc
        XrAction.cc
)
