/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EventQueue.hh"
#include "graphics/gui/FlatViewGuiContext.hh"

namespace sp {
    class GraphicsManager;
    class GpuTexture;

    enum class MenuScreen { Main, Options, SceneSelect, SaveSelect };

    class MenuGuiManager final : public FlatViewGuiContext {
    public:
        MenuGuiManager(GraphicsManager &graphics);

        void BeforeFrame() override;
        void DefineWindows() override;

        bool MenuOpen() const;
        void RefreshSceneList();
        void RefreshSaveList();

    private:
        GraphicsManager &graphics;

        ecs::EventQueueRef events = ecs::EventQueue::New();

        MenuScreen selectedScreen = MenuScreen::Main;
        std::vector<std::pair<std::string, std::string>> sceneList;
        std::vector<std::pair<std::string, std::string>> saveList;

        shared_ptr<GpuTexture> logoTex;
    };

    inline bool IsAspect(glm::ivec2 size, int w, int h) {
        return ((size.x * h) / w) == size.y;
    }
} // namespace sp
