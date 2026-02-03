/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/components/GuiElement.hh"

namespace sp {
    class FpsCounterGui final : public ecs::GuiDefinition {
    public:
        FpsCounterGui();
        virtual ~FpsCounterGui() {}

        bool BeforeFrame(GenericCompositor &compositor, ecs::Entity ent) override;
        void PreDefine(ecs::Entity ent) override;
        void DefineContents(ecs::Entity ent) override;
        void PostDefine(ecs::Entity ent) override;
    };
} // namespace sp
