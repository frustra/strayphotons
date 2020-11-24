#include "ecs/components/TriggerArea.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<TriggerArea>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) {
        auto &area = dst.Set<TriggerArea>(lock);
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "min") {
                area.boundsMin = sp::MakeVec3(param.second);
            } else if (param.first == "max") {
                area.boundsMax = sp::MakeVec3(param.second);
            } else if (param.first == "command") {
                area.command = param.second.get<string>();
            }
        }
        return true;
    }
} // namespace ecs
