/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Events.hh"
#include "graphics/gui/FlatViewGuiContext.hh"

namespace sp {
    class GraphicsContext;
    class ConsoleGui;
    class FpsCounterGui;

    class OverlayGuiManager final : public FlatViewGuiContext {
    public:
        OverlayGuiManager();

        void BeforeFrame() override;
        void DefineWindows() override;

    private:
        std::shared_ptr<ConsoleGui> consoleGui;
        std::shared_ptr<FpsCounterGui> fpsCounterGui;
        ecs::ComponentAddRemoveObserver<ecs::Gui> guiObserver;

        ecs::EventQueueRef events = ecs::EventQueue::New();

        struct GuiEntityContext {
            ecs::Entity entity;
            GuiContext::Ref window;
        };
        vector<GuiEntityContext> guis;
    };
} // namespace sp
