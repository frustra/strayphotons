/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "GuiContext.hh"
#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"

namespace sp {

    class WorldGuiManager : public GuiContext {
    public:
        WorldGuiManager(ecs::Entity gui, const std::string &name);

        void BeforeFrame() override;
        void DefineWindows() override;

    private:
        ecs::EntityRef guiEntity;

        ecs::EventQueueRef events = ecs::NewEventQueue();

        struct PointingState {
            ecs::Entity sourceEntity;
            glm::vec2 mousePos;
            bool mouseDown = false;
        };

        vector<PointingState> pointingStack;
    };
} // namespace sp
