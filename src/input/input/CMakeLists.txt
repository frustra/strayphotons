target_sources(
    ${PROJECT_INPUT_CORE_LIB}
    PRIVATE
        ActionSource.cc
        InputManager.cc
)

add_subdirectory(glfw)
