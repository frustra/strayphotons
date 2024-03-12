/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "game/Game.hh"

#ifdef SP_PHYSICS_SUPPORT_PHYSX
    #include "physx/PhysxManager.hh"
#endif

#include <cxxopts.hpp>

namespace sp {
    void InitPhysicsManager(Game &game) {
#ifdef SP_PHYSICS_SUPPORT_PHYSX
        game.physics = make_shared<PhysxManager>(game.inputEventQueue);
#endif
    }

    void StartPhysicsThread(Game &game, bool scriptMode) {
#ifdef SP_PHYSICS_SUPPORT_PHYSX
        if (game.physics) {
            game.physics->StartThread(scriptMode);
        }
#endif
    }
} // namespace sp
