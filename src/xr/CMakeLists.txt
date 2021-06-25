add_subdirectory(openvr)

target_sources(${PROJECT_EXE} PRIVATE
    XrCompositor.cc
    XrManager.cc
    XrSystemFactory.cc
    XrAction.cc
)

target_sources(${PROJECT_HEADLESS_EXE} PRIVATE
    XrCompositor.cc
    XrManager.cc
    XrSystemFactory.cc
    XrAction.cc
)

target_sources(${PROJECT_TEST_EXE} PRIVATE
    XrCompositor.cc
    XrManager.cc
    XrSystemFactory.cc
    XrAction.cc
)
