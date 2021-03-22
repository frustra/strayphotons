
target_sources(
    ${PROJECT_GRAPHICS_EGL_LIB}
    PRIVATE
        GuiRenderer.cc
        VoxelRenderer.cc
        RendererVoxels.cc
)

target_sources(
    ${PROJECT_GRAPHICS_OPENGL_LIB}
    PRIVATE
        GuiRenderer.cc
        VoxelRenderer.cc
        RendererVoxels.cc
)