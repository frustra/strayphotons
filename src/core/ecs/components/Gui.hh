#pragma once

#include "ecs/Components.hh"

namespace sp {
    class GuiContext;
}

namespace ecs {
    enum class GuiTarget {
        None,
        World,
        Debug,
    };

    struct Gui {
        GuiTarget target = GuiTarget::World;
        std::string windowName; // Must be set at component creation

        Gui() {}
        Gui(std::string windowName, GuiTarget target = GuiTarget::World) : windowName(windowName), target(target) {}
    };

    static Component<Gui> ComponentGui("gui",
        ComponentField::New("windowName", &Gui::windowName),
        ComponentField::New("target", &Gui::target));
} // namespace ecs
