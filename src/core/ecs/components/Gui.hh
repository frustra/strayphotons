#pragma once

#include "ecs/Components.hh"

namespace sp {
    class GuiContext;
}

namespace ecs {
    struct Gui {
        std::variant<sp::GuiContext *, std::string> target;
    };

    static Component<Gui> ComponentGui("gui");

    template<>
    bool Component<Gui>::Load(const EntityScope &scope, Gui &dst, const picojson::value &src);
} // namespace ecs
