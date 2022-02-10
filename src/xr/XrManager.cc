#ifdef SP_XR_SUPPORT

    #include "XrManager.hh"

    #include "core/Logging.hh"
    #include "main/Game.hh"

    #ifdef SP_XR_SUPPORT_OPENVR
        #include "xr/openvr/OpenVrSystem.hh"
    #endif

namespace sp::xr {
    XrManager::XrManager(Game *game) : game(game) {
        funcs.Register(this, "reloadxrsystem", "Reload the state of the XR subsystem", &XrManager::LoadXrSystem);
    }

    void XrManager::LoadXrSystem() {
        std::lock_guard lock(xrLoadMutex);

    #ifdef SP_XR_SUPPORT_OPENVR
        xrSystem = std::make_shared<OpenVrSystem>();
    #else
        Abort("No XR system defined");
    #endif

        if (!xrSystem->IsHmdPresent()) {
            Logf("No VR HMD is present.");
            return;
        }

        try {
            if (!xrSystem->Init(game->graphics.GetContext())) {
                Errorf("XR Runtime initialization failed!");
                xrSystem.reset();
            }
        } catch (const std::exception &ex) {
            Errorf("XR Runtime threw error on initialization! Error: %s", ex.what());
            xrSystem.reset();
        }
    }

    std::shared_ptr<XrSystem> XrManager::GetXrSystem() {
        std::lock_guard lock(xrLoadMutex);
        if (xrSystem && xrSystem->IsInitialized()) return xrSystem;
        return nullptr;
    }
} // namespace sp::xr

#endif
