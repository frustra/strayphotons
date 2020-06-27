add_subdirectory(basic_renderer)
add_subdirectory(postprocess)

target_sources(
    ${PROJECT_LIB}
    PRIVATE
        Buffer.cc
        GenericShaders.cc
        GPUTypes.cc
        Graphics.cc
        GraphicsManager.cc
        LightSensor.cc
        PixelFormat.cc
        RenderBuffer.cc
        RendererVoxels.cc
        RenderTarget.cc
        RenderTargetPool.cc
        SceneShaders.cc
        Shader.cc
        ShaderManager.cc
        Texture.cc
        Util.cc
)

target_sources(
    ${PROJECT_EXE}
    PRIVATE
        Renderer.cc ## Potentially move into a subfolder like basic_renderer
        GuiRenderer.cc
        GraphicsContext.cc
)

target_sources(
    ${PROJECT_TEST_EXE}
    PRIVATE
        Renderer.cc ## Potentially move into a subfolder like basic_renderer
        GuiRenderer.cc
        GraphicsContext.cc
)