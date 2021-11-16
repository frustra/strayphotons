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
            trigger = TriggerGroup::PLAYER;
        } else if (group == "OBJECT") {
            trigger = TriggerGroup::OBJECT;
        } else {
            Errorf("Unknown trigger group: %s", group);
            return false;
        }
        return true;
    }

    template<>
    bool Component<TriggerArea>::Load(sp::Scene *scene, TriggerArea &area, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "triggers") {
                for (auto groupObj : param.second.get<picojson::object>()) {
                    auto groupStr = sp::to_upper_copy(groupObj.first);
                    TriggerGroup group;
                    if (groupStr == "PLAYER") {
                        group = TriggerGroup::PLAYER;
                    } else if (groupStr == "OBJECT") {
                        group = TriggerGroup::OBJECT;
                    } else {
                        Errorf("Unknown trigger group: %s", groupStr);
                        return false;
                    }

                    auto &trigger = area.triggers[(size_t)group];
                    for (auto triggerParam : groupObj.second.get<picojson::object>()) {
                        if (triggerParam.first == "command") {
                            trigger.command = triggerParam.second.get<string>();
                        } else if (triggerParam.first == "signal") {
                            trigger.signalOutput = triggerParam.second.get<string>();
                        }
                    }
                }
            }
        }
        return true;
    }
} // namespace ecs
