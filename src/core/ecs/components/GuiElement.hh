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

namespace ecs {
    enum class GuiLayoutAnchor {
        Fullscreen,
        Top,
        Left,
        Right,
        Bottom,
        Floating,
    };

    class GuiDefinition {
    public:
        GuiDefinition(std::string_view name, int windowFlags = 0) : name(name), windowFlags(windowFlags) {}
        virtual ~GuiDefinition() {}

        virtual bool PreDefine(Entity ent) {
            return true;
        }
        virtual void DefineContents(Entity ent) = 0;
        virtual void PostDefine(Entity ent) {}

        const std::string name;
        int windowFlags = 0;
    };

    struct GuiElement {
        GuiLayoutAnchor anchor = GuiLayoutAnchor::Fullscreen;
        glm::ivec2 preferredSize = {-1, -1};
        bool enabled = true;
        std::shared_ptr<GuiDefinition> definition;

        GuiElement() = default;
        GuiElement(GuiLayoutAnchor anchor, glm::ivec2 preferredSize = {-1, -1}, bool enabled = false)
            : anchor(anchor), preferredSize(preferredSize), enabled(enabled) {}
        GuiElement(std::shared_ptr<GuiDefinition> definition,
            GuiLayoutAnchor anchor,
            glm::ivec2 preferredSize = {-1, -1},
            bool enabled = true)
            : anchor(anchor), preferredSize(preferredSize), enabled(enabled), definition(definition) {}
    };

    static EntityComponent<GuiElement> ComponentGuiElement("gui_element",
        "",
        StructField::New("enabled", &GuiElement::enabled),
        StructField::New("anchor", &GuiElement::anchor),
        StructField::New("preferred_size", &GuiElement::preferredSize));

    template<>
    void EntityComponent<GuiElement>::Apply(GuiElement &dst, const GuiElement &src, bool liveTarget);
} // namespace ecs
