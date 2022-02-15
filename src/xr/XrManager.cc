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

        {
            // ensure old system shuts down before initializing a new one
            auto oldSystem = xrSystem;
            xrSystem.reset();
            while (oldSystem.use_count() > 1)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

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
