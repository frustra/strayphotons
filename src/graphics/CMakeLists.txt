add_subdirectory(basic_renderer)
add_subdirectory(postprocess)
add_subdirectory(raytrace)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        Buffer.cc
        GenericShaders.cc
        GPUTypes.cc
        Graphics.cc
        GraphicsContext.cc
        GraphicsManager.cc
        GuiRenderer.cc
        LightSensor.cc
        PixelFormat.cc
        RenderBuffer.cc
        Renderer.cc ## Potentially move into a subfolder like basic_renderer and raytrace
        RendererVoxels.cc
        RenderTarget.cc
        RenderTargetPool.cc
        SceneShaders.cc
        Shader.cc
        ShaderManager.cc
        Texture.cc
        Util.cc
)