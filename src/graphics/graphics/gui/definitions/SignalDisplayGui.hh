/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EntityRef.hh"
#include "graphics/gui/GuiContext.hh"

namespace sp {
    class SignalDisplayGui : public GuiRenderable {
    public:
        SignalDisplayGui(const std::string &name, const ecs::Entity &ent);
        virtual ~SignalDisplayGui() {}

        bool PreDefine() override;
        void DefineContents() override;
        void PostDefine() override;

    private:
        ecs::EntityRef signalEntity;
    };
} // namespace sp
