#include "Gui.hh"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Gui>::Load(const EntityScope &scope, Gui &dst, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "window") { dst.target = param.second.get<string>(); }
        }
        if (!std::holds_alternative<std::string>(dst.target)) {
            Errorf("no window specified");
            return false;
        }
        return true;
    }
} // namespace ecs
