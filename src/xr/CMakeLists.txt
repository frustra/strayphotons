add_subdirectory(openvr)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        XrCompositor.cc
        XrSystemFactory.cc
        XrAction.cc
        XrManager.cc
)
