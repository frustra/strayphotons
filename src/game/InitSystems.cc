/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "game/Game.hh"

#ifdef SP_AUDIO_SUPPORT
    #include "audio/AudioManager.hh"
#endif

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/core/GraphicsManager.hh"
#endif

#ifdef SP_GRAPHICS_SUPPORT_VK
    #include "graphics/vulkan/core/DeviceContext.hh"
#endif

#ifdef SP_PHYSICS_SUPPORT_PHYSX
    #include "physx/PhysxManager.hh"
#endif

#ifdef SP_XR_SUPPORT_OPENVR
    #include "openvr/OpenVrSystem.hh"
#endif

#ifdef SP_RUST_WASM_SUPPORT
    #include <wasm.rs.h>
#endif

#include <cxxopts.hpp>

namespace sp {
    void InitAudioManager(Game &game) {
#ifdef SP_AUDIO_SUPPORT
        game.audio = make_shared<AudioManager>();
#endif
    }

    void InitGraphicsManager(Game &game) {
#ifdef SP_GRAPHICS_SUPPORT
        if (!game.options.count("headless")) {
            game.graphics = make_shared<GraphicsManager>(game);
        }
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

    void LoadXrSystem(Game &game) {
#ifdef SP_XR_SUPPORT_OPENVR
        game.xr = std::make_shared<xr::OpenVrSystem>(game.graphics->context.get());

        game.funcs.Register("reloadxrsystem", "Reload the state of the XR subsystem", [&] {
            // ensure old system shuts down before initializing a new one
            std::weak_ptr<xr::XrSystem> xrSystem = game.xr;
            game.xr.reset();
            while (!xrSystem.expired()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            game.xr = std::make_shared<xr::OpenVrSystem>(game.graphics->context.get());
        });
#endif
    }

    void InitRust(Game &game) {
#ifdef SP_RUST_WASM_SUPPORT
        wasm::print_hello();
#endif
    }
} // namespace sp
