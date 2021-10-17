#pragma once

#ifdef SP_XR_SUPPORT

    #include "core/console/CFunc.hh"
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

    #ifdef SP_GRAPHICS_SUPPORT_VK

    #endif

    private:
        sp::Game *game = nullptr;
        CFuncCollection funcs;

        std::shared_ptr<XrSystem> xrSystem;
    };

} // namespace sp::xr

#endif
