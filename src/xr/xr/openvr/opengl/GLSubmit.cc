#include "graphics/core/Texture.hh"
#include "graphics/opengl/GLTexture.hh"
#include "xr/openvr/OpenVrSystem.hh"

#include <openvr.h>

namespace sp::xr {
    void OpenVrSystem::SubmitView(ecs::XrEye eye, GpuTexture *tex) {
        Assert(tex != nullptr, "TranslateTexture: null GpuTexture");

        GLTexture *glTex = dynamic_cast<GLTexture *>(tex);
        Assert(glTex != nullptr, "TranslateTexture: GpuTexture is not a GLTexture");

        vr::Texture_t texture = {(void *)(size_t)glTex->handle, vr::TextureType_OpenGL, vr::ColorSpace_Linear};

        vr::VRCompositor()->Submit(MapXrEyeToOpenVr(eye), &texture);
    }
} // namespace sp::xr
