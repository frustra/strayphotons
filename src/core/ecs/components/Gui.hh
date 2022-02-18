#pragma once

#include "ecs/Components.hh"

namespace sp {
    class GuiManager;
}

namespace ecs {
    struct Gui {
        sp::GuiManager *manager;
    };

    static Component<Gui> ComponentGui("gui");
} // namespace ecs
