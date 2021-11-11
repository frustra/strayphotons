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
            auto &area = entity.Get<ecs::TriggerArea>(lock);

            for (auto triggerEnt : lock.EntitiesWith<ecs::TriggerGroup>()) {
                if (!triggerEnt.Has<ecs::TriggerGroup, ecs::Transform>(lock)) continue;

                auto triggerGroup = triggerEnt.Get<ecs::TriggerGroup>(lock);
                Assert(triggerGroup < ecs::TriggerGroup::COUNT, "Entity has invalid trigger group");
                auto &trigger = area.triggers[(size_t)triggerGroup];

                auto &transform = triggerEnt.Get<ecs::Transform>(lock);
                auto entityPos = transform.GetGlobalPosition(lock);
                bool inArea = entityPos.x > area.boundsMin.x && entityPos.y > area.boundsMin.y &&
                              entityPos.z > area.boundsMin.z && entityPos.x < area.boundsMax.x &&
                              entityPos.y < area.boundsMax.y && entityPos.z < area.boundsMax.z;
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
