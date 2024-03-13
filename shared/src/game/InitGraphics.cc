/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "game/Game.hh"

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/core/GraphicsManager.hh"
#endif

#ifdef SP_GRAPHICS_SUPPORT_VK
    #include "graphics/vulkan/core/DeviceContext.hh"
#endif

#include <cxxopts.hpp>

namespace sp {
    void InitGraphicsManager(Game &game) {
#ifdef SP_GRAPHICS_SUPPORT
        game.graphics = make_shared<GraphicsManager>(game);
#endif
    }

    void StartGraphicsThread(Game &game, bool scriptMode) {
#ifdef SP_GRAPHICS_SUPPORT
        if (game.graphics) {
            game.graphics->Init();

    #ifdef SP_GRAPHICS_SUPPORT_VK
            bool withValidationLayers = game.options.count("with-validation-layers");
            game.graphics->context = std::make_shared<vulkan::DeviceContext>(*game.graphics, withValidationLayers);
    #endif

            game.graphics->StartThread(scriptMode);
        }
#endif
    }
} // namespace sp
