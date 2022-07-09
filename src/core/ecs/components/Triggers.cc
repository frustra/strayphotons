#include "Triggers.hh"

#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<TriggerGroup>::Load(const EntityScope &scope, TriggerGroup &trigger, const picojson::value &src) {
        auto group = src.get<std::string>();
        sp::to_upper(group);
        if (group == "PLAYER") {
            trigger = TriggerGroup::Player;
        } else if (group == "OBJECT") {
            trigger = TriggerGroup::Object;
        } else {
            Errorf("Unknown trigger group: %s", group);
            return false;
        }
        return true;
    }

    template<>
    bool Component<TriggerArea>::Load(const EntityScope &scope, TriggerArea &area, const picojson::value &src) {
        return true;
    }
} // namespace ecs
