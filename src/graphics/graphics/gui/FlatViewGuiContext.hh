/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EventQueue.hh"
#include "graphics/gui/GuiContext.hh"

namespace sp {
    class FlatViewGuiContext : public GuiContext {
    public:
        FlatViewGuiContext(const std::string &name);

        virtual void BeforeFrame();

    protected:
        ecs::EntityRef guiEntity;
        ecs::EventQueueRef events = ecs::EventQueue::New();
    };
} // namespace sp
