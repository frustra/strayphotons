/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/InlineString.hh"
#include "ecs/Components.hh"

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
        glm::vec2 scale = {-1, -1}; // -1 == use window ui scale
        std::vector<EntityRef> guiElements;
        // - effects? (blur, crosshair, other post-processing)
        // - sprites? (transform tree based positioning)

        std::shared_ptr<sp::GuiContext> guiContext; // TODO: shared_ptr factory or raw pointer manual lifetime?

        RenderOutput() {}
        RenderOutput(std::string sourceName, std::initializer_list<EntityRef> guiElements = {})
            : sourceName(sourceName), guiElements(guiElements) {}
    };

    static EntityComponent<RenderOutput> ComponentRenderOutput("render_output",
        "",
        StructField::New("source", &RenderOutput::sourceName),
        StructField::New("output_size", &RenderOutput::outputSize),
        StructField::New("scale", &RenderOutput::scale),
        StructField::New("gui_elements", &RenderOutput::guiElements));
} // namespace ecs
