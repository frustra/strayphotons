
target_sources(${PROJECT_GRAPHICS_EGL_LIB} PRIVATE
    GenericShaders.cc
    GLBuffer.cc
    GlfwGraphicsContext.cc
    GLModel.cc
    GLRenderTarget.cc
    GLTexture.cc
    GPUTypes.cc
    Graphics.cc
    LightSensor.cc
    PerfTimer.cc
    PixelFormat.cc
    RenderBuffer.cc
    RenderTargetPool.cc
    SceneShaders.cc
    Shader.cc
    ShaderManager.cc
)

target_sources(${PROJECT_GRAPHICS_OPENGL_LIB} PRIVATE
    GenericShaders.cc
    GLBuffer.cc
    GlfwGraphicsContext.cc
    GLModel.cc
    GLRenderTarget.cc
    GLTexture.cc
    GPUTypes.cc
    Graphics.cc
    LightSensor.cc
    PerfTimer.cc
    PixelFormat.cc
    RenderBuffer.cc
    RenderTargetPool.cc
    SceneShaders.cc
    Shader.cc
    ShaderManager.cc
)

add_subdirectory(basic_renderer)
add_subdirectory(gui)
add_subdirectory(postprocess)
add_subdirectory(voxel_renderer)
