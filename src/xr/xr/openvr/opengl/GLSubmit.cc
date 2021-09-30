#if defined(SP_XR_SUPPORT_OPENVR) && defined(SP_GRAPHICS_SUPPORT_GL)

    #include "graphics/core/Texture.hh"
    #include "graphics/opengl/GLTexture.hh"
    #include "xr/openvr/OpenVrSystem.hh"

    #include <openvr.h>

namespace sp::xr {
    void OpenVrSystem::SubmitView(ecs::XrEye eye, glm::mat4 &viewPose, GpuTexture *tex) {
        Assert(tex != nullptr, "TranslateTexture: null GpuTexture");

        GLTexture *glTex = dynamic_cast<GLTexture *>(tex);
        Assert(glTex != nullptr, "TranslateTexture: GpuTexture is not a GLTexture");

        vr::VRTextureWithPose_t texture;
        texture.handle = (void *)(size_t)glTex->handle;
        texture.eType = vr::TextureType_OpenGL;
        texture.eColorSpace = vr::ColorSpace_Auto;
        glm::mat3x4 trackingPose = glm::transpose(viewPose);
        memcpy((float *)texture.mDeviceToAbsoluteTracking.m,
               glm::value_ptr(trackingPose),
               sizeof(texture.mDeviceToAbsoluteTracking.m));

        vr::VRCompositor()->Submit(MapXrEyeToOpenVr(eye), &texture, 0, vr::EVRSubmitFlags::Submit_TextureWithPose);
    }
} // namespace sp::xr

#endif
