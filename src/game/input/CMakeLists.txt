target_sources(
    ${PROJECT_LIB}
    PRIVATE
        InputManager.cc
)

target_sources(
    ${PROJECT_EXE}
    PRIVATE
        GlfwInputManager.cc
)

target_sources(
    ${PROJECT_TEST_EXE}
    PRIVATE
        GlfwInputManager.cc
)
