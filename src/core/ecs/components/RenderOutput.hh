/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

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

    enum class GuiTarget {
        None = 0,
        World,
        Overlay,
    };

    struct RenderOutput {
        GuiTarget target = GuiTarget::World;
        // std::variant<std::monostate, ScriptGuiElement> source;

        RenderOutput() {}
        RenderOutput(std::string windowName, GuiTarget target = GuiTarget::World) : target(target) {}
    };

    static EntityComponent<RenderOutput> ComponentRenderOutput("render_output",
        "",
        StructField::New("target", &RenderOutput::target));
} // namespace ecs
