/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "GuiContext.hh"
#include "ecs/EventQueue.hh"

namespace sp {

    class SystemGuiManager : public GuiContext {
    public:
        SystemGuiManager(const std::string &name);

        virtual void BeforeFrame();

    protected:
        ecs::EntityRef guiEntity;
        ecs::EventQueueRef events = ecs::NewEventQueue();
    };
} // namespace sp
