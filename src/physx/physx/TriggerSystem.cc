#include "TriggerSystem.hh"

#include "console/Console.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

namespace sp {
    void TriggerSystem::Frame() {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::TriggerGroup, ecs::Transform>,
            ecs::Write<ecs::TriggerArea, ecs::SignalOutput>>();

        for (auto &entity : lock.EntitiesWith<ecs::TriggerArea>()) {
            if (!entity.Has<ecs::TriggerArea, ecs::Transform>(lock)) continue;
            auto &area = entity.Get<ecs::TriggerArea>(lock);
            glm::mat4 areaTransform = glm::inverse(entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock));

            for (auto triggerEnt : lock.EntitiesWith<ecs::TriggerGroup>()) {
                if (!triggerEnt.Has<ecs::TriggerGroup, ecs::Transform>(lock)) continue;

                auto &trigger = area.triggers[triggerEnt.Get<ecs::TriggerGroup>(lock)];

                auto &transform = triggerEnt.Get<ecs::Transform>(lock);
                auto entityPos = glm::vec3(areaTransform * glm::vec4(transform.GetGlobalPosition(lock), 1.0));
                bool inArea = glm::all(glm::greaterThan(entityPos, glm::vec3(-0.5))) &&
                              glm::all(glm::lessThan(entityPos, glm::vec3(0.5)));
                if (trigger.entities.contains(triggerEnt) == inArea) continue;

                if (inArea) {
                    trigger.entities.insert(triggerEnt);

                    if (!trigger.command.empty()) {
                        Logf("TriggerArea running command: %s", trigger.command);
                        Debugf("Entity at: %f %f %f", entityPos.x, entityPos.y, entityPos.z);
                        GetConsoleManager().QueueParseAndExecute(trigger.command);
                    }
                } else {
                    trigger.entities.erase(triggerEnt);
                }
            }

            for (auto &trigger : area.triggers) {
                if (!trigger.signalOutput.empty() && entity.Has<ecs::SignalOutput>(lock)) {
                    auto &signalOutput = entity.Get<ecs::SignalOutput>(lock);
                    signalOutput.SetSignal(trigger.signalOutput, trigger.entities.size() > 0 ? 1.0 : 0.0);
                }
            }
        }
    }
} // namespace sp
