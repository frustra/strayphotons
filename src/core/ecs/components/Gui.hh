#pragma once

#include "ecs/Components.hh"

namespace sp {
    class GuiContext;
}

namespace ecs {
    struct Gui {
        sp::GuiContext *context = nullptr;
        std::string target; // Must be set at component creation
        bool disabled = false;

        Gui(sp::GuiContext *context = nullptr) : context(context) {}
        Gui(std::string targetName) : target(targetName) {}
    };

    static Component<Gui> ComponentGui("gui", ComponentField::New("target", &Gui::target));
} // namespace ecs
