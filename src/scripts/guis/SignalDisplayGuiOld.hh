/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EntityRef.hh"
#include "ecs/components/GuiElement.hh"

namespace sp {
    class SignalDisplayGui final : public ecs::GuiDefinition {
    public:
        SignalDisplayGui(const std::string &name);
        virtual ~SignalDisplayGui() {}

        void PreDefine(ecs::Entity ent) override;
        void DefineContents(ecs::Entity ent) override;
        void PostDefine(ecs::Entity ent) override;
    };
} // namespace sp
