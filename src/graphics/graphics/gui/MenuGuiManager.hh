/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EventQueue.hh"
#include "graphics/gui/SystemGuiManager.hh"

namespace sp {
    class GraphicsManager;
    class GpuTexture;

    enum class MenuScreen { Main, Options, SceneSelect };

    class MenuGuiManager : public SystemGuiManager {
    public:
        MenuGuiManager(GraphicsManager &graphics);
        virtual ~MenuGuiManager() {}

        void BeforeFrame() override;
        void DefineWindows() override;

        bool MenuOpen() const;

    private:
        GraphicsManager &graphics;

        ecs::EventQueueRef events = ecs::NewEventQueue();

        MenuScreen selectedScreen = MenuScreen::Main;

        shared_ptr<GpuTexture> logoTex;
    };

    inline bool IsAspect(glm::ivec2 size, int w, int h) {
        return ((size.x * h) / w) == size.y;
    }
} // namespace sp
