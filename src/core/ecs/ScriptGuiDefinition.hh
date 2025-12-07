/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/components/GuiElement.hh"
#include "graphics/GenericCompositor.hh"

#include <memory>

namespace ecs {
    class ScriptGuiDefinition final : public GuiDefinition {
    public:
        ScriptGuiDefinition(std::shared_ptr<ScriptState> state, EntityRef guiDefinitionEntity);
        ~ScriptGuiDefinition();

        bool BeforeFrame(Entity ent) override;
        void PreDefine(Entity ent) override;
        void DefineContents(Entity ent) override;
        void PostDefine(Entity ent) override;

        std::weak_ptr<ScriptState> weakState;
        EntityRef guiDefinitionEntity;

        struct CallbackContext {
            sp::GuiDrawData drawData;
            sp::GenericCompositor *compositor = nullptr;
            glm::vec4 viewport;
            glm::vec2 scale;
        } context = {};
    };
} // namespace ecs
