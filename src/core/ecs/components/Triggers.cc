#include "Triggers.hh"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<TriggerGroup>::Load(ScenePtr scenePtr, const Name &scope, TriggerGroup &trigger, const picojson::value &src) {
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
    bool Component<TriggerArea>::Load(ScenePtr scenePtr, const Name &scope, TriggerArea &area, const picojson::value &src) {
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

    template<>
    void Component<TriggerArea>::Apply(const TriggerArea &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstArea = dst.Get<TriggerArea>(lock);
        for (size_t i = 0; i < src.triggers.size(); i++) {
            auto &srcTriggers = src.triggers.at(i);
            auto &dstTriggers = dstArea.triggers.at(i);
            for (auto &trigger : srcTriggers) {
                if (std::find(dstTriggers.begin(), dstTriggers.end(), trigger) == dstTriggers.end()) {
                    dstTriggers.emplace_back(trigger);
                }
            }
        }
    }
} // namespace ecs
