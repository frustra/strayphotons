/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/LockFreeMutex.hh"
#include "console/CFunc.hh"
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
