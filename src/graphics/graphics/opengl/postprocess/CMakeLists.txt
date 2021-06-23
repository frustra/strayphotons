
target_sources(
    ${PROJECT_GRAPHICS_EGL_LIB}
    PRIVATE
        Bloom.cc
        Crosshair.cc
        GammaCorrect.cc
        Lighting.cc
        MenuGui.cc
        PostProcess.cc
        SMAA.cc
        SSAO.cc
        ViewGBuffer.cc
)

target_sources(
    ${PROJECT_GRAPHICS_OPENGL_LIB}
    PRIVATE
        Bloom.cc
        Crosshair.cc
        GammaCorrect.cc
        Lighting.cc
        MenuGui.cc
        PostProcess.cc
        SMAA.cc
        SSAO.cc
        ViewGBuffer.cc
)
