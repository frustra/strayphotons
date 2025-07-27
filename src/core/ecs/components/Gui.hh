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

namespace ecs {
    enum class GuiTarget {
        None = 0,
        World,
        Overlay,
    };

    enum class GuiLayoutAnchor {
        Fullscreen,
        Top,
        Left,
        Right,
        Bottom,
        Floating,
    };

    class GuiRenderable {
    public:
        GuiRenderable(std::string_view name,
            GuiLayoutAnchor anchor,
            glm::ivec2 preferredSize = {-1, -1},
            int windowFlags = 0)
            : name(name), anchor(anchor), preferredSize(preferredSize), windowFlags(windowFlags) {}
        virtual ~GuiRenderable() {}

        virtual bool PreDefine(Entity ent) {
            return true;
        }
        virtual void DefineContents(Entity ent) = 0;
        virtual void PostDefine(Entity ent) {}

        const std::string name;
        GuiLayoutAnchor anchor;
        glm::ivec2 preferredSize;
        int windowFlags = 0;
    };

    struct Gui {
        GuiTarget target = GuiTarget::World;
        std::string windowName; // Must be set at component creation

        Gui() {}
        Gui(std::string windowName, GuiTarget target = GuiTarget::World) : target(target), windowName(windowName) {}
    };

    static EntityComponent<Gui> ComponentGui("gui",
        "",
        StructField::New("window_name", &Gui::windowName),
        StructField::New("target", &Gui::target));
} // namespace ecs
