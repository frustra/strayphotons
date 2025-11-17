/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EventQueue.hh"
#include "graphics/gui/GuiContext.hh"

#include <memory>

namespace sp {
    class ConsoleGui;
    class FpsCounterGui;

    class OverlayGuiManager final : public GuiContext {
    public:
        OverlayGuiManager(OverlayGuiManager &&) = default;
        virtual ~OverlayGuiManager();

        static std::shared_ptr<GuiContext> CreateContext(const ecs::Name &guiName);

        bool BeforeFrame() override;
        void DefineWindows() override;

    private:
        OverlayGuiManager(const ecs::EntityRef &guiEntity);

        std::shared_ptr<ConsoleGui> consoleGui;
        std::shared_ptr<FpsCounterGui> fpsCounterGui;

        ecs::EventQueueRef events = ecs::EventQueue::New();
    };
} // namespace sp
