target_sources(
    ${PROJECT_GRAPHICS_CORE_LIB}
    PRIVATE
        DebugGuiManager.cc
        MenuGuiManager.cc
)

target_sources(
    ${PROJECT_GRAPHICS_OPENGL_LIB}
    PRIVATE
        GuiManager.cc
)

target_sources(
    ${PROJECT_GRAPHICS_EGL_LIB}
    PRIVATE
        GuiManager.cc
)
