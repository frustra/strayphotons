/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EventQueue.hh"
#include "graphics/GenericCompositor.hh"
#include "graphics/core/GraphicsManager.hh"
#include "graphics/core/Texture.hh"
#include "gui/GuiContext.hh"

#include <memory>

namespace sp {
    class Game;
    class AudioManager;
    class GraphicsManager;
    class GpuTexture;

    enum class MenuScreen { Main, Options, SceneSelect, SaveSelect };

    class MenuGuiManager final : public GuiContext {
    public:
        MenuGuiManager(MenuGuiManager &&) = default;
        virtual ~MenuGuiManager();

        static std::shared_ptr<GuiContext> CreateContext(const ecs::Name &guiName, Game &game);

        bool BeforeFrame(GenericCompositor &compositor) override;
        void DefineWindows() override;

        bool MenuOpen() const;
        void RefreshSceneList();
        void RefreshSaveList();

    private:
        MenuGuiManager(const ecs::EntityRef &guiEntity, Game &game);

        std::weak_ptr<AudioManager> audioPtr;
        std::weak_ptr<GraphicsManager> graphicsPtr;

        ecs::EventQueueRef events = ecs::EventQueue::New();

        MenuScreen selectedScreen = MenuScreen::Main;
        std::vector<std::pair<std::string, std::string>> sceneList;
        std::vector<std::pair<std::string, std::string>> saveList;

        std::shared_ptr<GpuTexture> logoTex;
        GenericCompositor::ResourceID logoResourceID = GenericCompositor::InvalidResource;
    };

    inline bool IsAspect(glm::ivec2 size, int w, int h) {
        return ((size.x * h) / w) == size.y;
    }
} // namespace sp
