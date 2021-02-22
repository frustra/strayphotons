add_subdirectory(basic_renderer)
add_subdirectory(voxel_renderer)
add_subdirectory(glfw_graphics_context)
add_subdirectory(postprocess)

target_sources(
    ${PROJECT_GRAPHICS_CORE_LIB}
    PRIVATE
        Buffer.cc
        GenericShaders.cc
        GPUTypes.cc
        Graphics.cc
        GraphicsContext.cc
        LightSensor.cc
        PixelFormat.cc
        Renderer.cc
        RenderBuffer.cc
        RenderTarget.cc
        RenderTargetPool.cc
        SceneShaders.cc
        Shader.cc
        ShaderManager.cc
        Texture.cc
        Util.cc
)

target_sources(
    ${PROJECT_GRAPHICS_EGL_LIB}
    PRIVATE
        GraphicsManager.cc
)

target_sources(
    ${PROJECT_GRAPHICS_OPENGL_LIB}
    PRIVATE
        GraphicsManager.cc
)