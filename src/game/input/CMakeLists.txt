target_sources(
    ${PROJECT_LIB}
    PRIVATE
        ActionSource.cc
        InputManager.cc
)

target_sources(
    ${PROJECT_EXE}
    PRIVATE
        GlfwActionSource.cc
)

target_sources(
    ${PROJECT_TEST_EXE}
    PRIVATE
        GlfwActionSource.cc
)
