#pragma once

#ifdef SP_XR_SUPPORT

    #include "console/CFunc.hh"
    #include "core/LockFreeMutex.hh"
    #include "xr/XrSystem.hh"

    #include <memory>

namespace sp {
    class Game;
}

namespace sp::xr {

    class XrManager {
    public:
        XrManager(sp::Game *game);

        void LoadXrSystem();

        std::shared_ptr<XrSystem> GetXrSystem();

    private:
        sp::Game *game = nullptr;
        CFuncCollection funcs;

        LockFreeMutex xrLoadMutex;
        std::shared_ptr<XrSystem> xrSystem;
    };

} // namespace sp::xr

#endif
