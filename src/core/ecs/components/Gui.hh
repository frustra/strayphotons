/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"

namespace sp {
    class GuiContext;
}

namespace ecs {
    enum class GuiTarget {
        None = 0,
        World,
        Overlay,
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
