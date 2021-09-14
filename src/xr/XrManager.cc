#ifdef SP_XR_SUPPORT

    #include "XrManager.hh"

    #include "core/Logging.hh"
    #include "game/Game.hh"

    #ifdef SP_XR_SUPPORT_OPENVR
        #include "graphics/opengl/GlfwGraphicsContext.hh"
        #include "xr/openvr/OpenVrSystem.hh"
    #endif

namespace sp::xr {
    static CVar<bool> CVarXrEnabled("xr.Enabled", true, "Enable the XR/VR system on launch");

    std::shared_ptr<XrSystem> XrManager::LoadXrSystem() {
        if (!xrSystem) {
    #ifdef SP_XR_SUPPORT_OPENVR
            GlfwGraphicsContext *glContext = dynamic_cast<GlfwGraphicsContext *>(game->graphics.GetContext());
            Assert(glContext, "OpenVrSystem requires a GLFW Graphics Context");
            xrSystem = std::make_shared<OpenVrSystem>(*glContext);
    #endif
        }

        if (!xrSystem->IsHmdPresent()) {
            Logf("No VR HMD is present.");
            return nullptr;
        }

        return xrSystem;
    }
} // namespace sp::xr

#endif
