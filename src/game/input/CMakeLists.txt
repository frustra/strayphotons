target_sources(
    ${PROJECT_LIB}
    PRIVATE
        ActionSource.cc
        InputManager.cc
)

target_sources(
    ${PROJECT_GRAPHICS_EGL_LIB}
    PRIVATE
        GlfwActionSource.cc
)

target_sources(
    ${PROJECT_GRAPHICS_OPENGL_LIB}
    PRIVATE
        GlfwActionSource.cc
)
