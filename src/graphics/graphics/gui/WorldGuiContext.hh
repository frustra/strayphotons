/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"
#include "graphics/gui/GuiContext.hh"

namespace sp {
    class WorldGuiContext : public GuiContext {
    public:
        WorldGuiContext(ecs::Entity gui, const std::string &name);

        void BeforeFrame() override;
        void DefineWindows() override;

    private:
        ecs::EntityRef guiEntity;

        ecs::EventQueueRef events = ecs::EventQueue::New();

        struct PointingState {
            ecs::Entity sourceEntity;
            glm::vec2 mousePos;
            bool mouseDown = false;
        };

        vector<PointingState> pointingStack;
    };
} // namespace sp
