#pragma once

#include "ecs/Components.hh"

namespace sp {
    class GuiManager;
}

namespace ecs {
    struct Gui {
        float luminanceScale = 1.0f;
        sp::GuiManager *manager;
    };

    static Component<Gui> ComponentGui("gui");
} // namespace ecs
