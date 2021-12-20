#include "Triggers.hh"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<TriggerGroup>::Load(sp::Scene *scene, TriggerGroup &trigger, const picojson::value &src) {
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
    bool Component<TriggerArea>::Load(sp::Scene *scene, TriggerArea &area, const picojson::value &src) {
        for (auto groupObj : src.get<picojson::object>()) {
            auto groupStr = sp::to_upper_copy(groupObj.first);
            TriggerGroup group;
            if (groupStr == "PLAYER") {
                group = TriggerGroup::Player;
            } else if (groupStr == "OBJECT") {
                group = TriggerGroup::Object;
            } else {
                Errorf("Unknown trigger group: %s", groupStr);
                return false;
            }

            auto &triggers = area.triggers[group];
            for (auto triggerParam : groupObj.second.get<picojson::object>()) {
                if (triggerParam.first == "command") {
                    triggers.emplace_back(TriggerArea::TriggerCommand(triggerParam.second.get<string>()));
                } else if (triggerParam.first == "signal") {
                    triggers.emplace_back(TriggerArea::TriggerSignal(triggerParam.second.get<string>()));
                }
            }
        }
        return true;
    }
} // namespace ecs
