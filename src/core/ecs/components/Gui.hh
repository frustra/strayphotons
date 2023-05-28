#pragma once

#include "ecs/Components.hh"

namespace sp {
    class GuiContext;
}

namespace ecs {
    enum class GuiTarget {
        None = 0,
        World,
        Debug,
    };

    struct Gui {
        GuiTarget target = GuiTarget::World;
        std::string windowName; // Must be set at component creation

        Gui() {}
        Gui(std::string windowName, GuiTarget target = GuiTarget::World) : target(target), windowName(windowName) {}
    };

    static StructMetadata MetadataGui(typeid(Gui),
        "gui",
        StructField::New("windowName", &Gui::windowName),
        StructField::New("target", &Gui::target));
    static Component<Gui> ComponentGui(MetadataGui);
} // namespace ecs
