/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Events.hh"
#include "graphics/gui/SystemGuiManager.hh"

namespace sp {
    class GraphicsContext;

    class DebugGuiManager : public SystemGuiManager {
    public:
        DebugGuiManager();
        virtual ~DebugGuiManager() {}

        void BeforeFrame() override;
        void DefineWindows() override;

    private:
        void AddGui(ecs::Entity ent, const ecs::Gui &gui);

        bool consoleOpen = false;
        ecs::ComponentObserver<ecs::Gui> guiObserver;

        ecs::EventQueueRef events = ecs::NewEventQueue();

        struct GuiEntityContext {
            ecs::Entity entity;
            shared_ptr<GuiWindow> window = nullptr;
        };
        vector<GuiEntityContext> guis;
    };
} // namespace sp
