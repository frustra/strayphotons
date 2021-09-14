#pragma once

#ifdef SP_XR_SUPPORT

    #include "xr/XrSystem.hh"

    #include <memory>

namespace sp {
    class Game;
}

namespace sp::xr {

    class XrManager {
    public:
        XrManager(sp::Game *game) : game(game) {}

        std::shared_ptr<XrSystem> LoadXrSystem();

    private:
        sp::Game *game = nullptr;
        std::shared_ptr<XrSystem> xrSystem;
    };

} // namespace sp::xr

#endif
