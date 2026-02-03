/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/InlineString.hh"
#include "ecs/Components.hh"
#include "ecs/SignalExpression.hh"

#include <glm/glm.hpp>
#include <memory>
#include <span>

namespace sp {
    class GuiContext;
}

namespace sp::vulkan {
    class Renderer;
}

namespace ecs {
    class ScriptState;

    struct RenderOutput {
        sp::InlineString<127> sourceName;
        glm::ivec2 outputSize = {-1, -1}; // -1 == inherit
        glm::vec2 scale = {-1, -1}; // -1 == inherit
        sp::InlineString<127> effectName;
        SignalExpression effectCondition;
        std::vector<EntityRef> guiElements;
        // - sprites? (transform tree based positioning)

        std::weak_ptr<sp::GuiContext> guiContext;

        RenderOutput() {}
        RenderOutput(std::string sourceName, std::initializer_list<EntityRef> guiElements = {})
            : sourceName(sourceName), guiElements(guiElements) {}
    };

    static EntityComponent<RenderOutput> ComponentRenderOutput("render_output",
        "",
        StructField::New("source", &RenderOutput::sourceName),
        StructField::New("output_size", &RenderOutput::outputSize),
        StructField::New("scale", &RenderOutput::scale),
        StructField::New("effect", &RenderOutput::effectName),
        StructField::New("effect_if", &RenderOutput::effectCondition),
        StructField::New("gui_elements", &RenderOutput::guiElements, ~FieldAction::AutoApply));
    template<>
    void EntityComponent<RenderOutput>::Apply(RenderOutput &dst, const RenderOutput &src, bool liveTarget);
} // namespace ecs
