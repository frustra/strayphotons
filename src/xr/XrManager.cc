#ifdef SP_XR_SUPPORT

    #include "XrManager.hh"

    #include "core/Logging.hh"
    #include "core/Tracing.hh"
    #include "main/Game.hh"

    #ifdef SP_XR_SUPPORT_OPENVR
        #include "xr/openvr/OpenVrSystem.hh"
    #endif

namespace sp::xr {
    XrManager::XrManager(Game *game) : game(game) {
        funcs.Register(this, "reloadxrsystem", "Reload the state of the XR subsystem", &XrManager::LoadXrSystem);
    }

    void XrManager::LoadXrSystem() {
        ZoneScoped;
        std::lock_guard lock(xrLoadMutex);

    #ifdef SP_XR_SUPPORT_OPENVR
        xrSystem = std::make_shared<OpenVrSystem>(game->graphics.GetContext());
    #else
        Abort("No XR system defined");
    #endif
    }

    std::shared_ptr<XrSystem> XrManager::GetXrSystem() {
        std::lock_guard lock(xrLoadMutex);
        return xrSystem;
    }
} // namespace sp::xr

#endif
