#include "TriggerSystem.hh"

#include "console/Console.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

namespace sp {
    TriggerSystem::TriggerSystem() {
        auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
        triggerGroupObserver = lock.Watch<ecs::ComponentEvent<ecs::TriggerGroup>>();
    }

    TriggerSystem::~TriggerSystem() {
        auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
        triggerGroupObserver.Stop(lock);
    }

    void TriggerSystem::Frame() {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::TriggerGroup, ecs::Transform>,
            ecs::Write<ecs::TriggerArea, ecs::SignalOutput>>();

        for (auto &entity : lock.EntitiesWith<ecs::TriggerArea>()) {
            if (!entity.Has<ecs::TriggerArea, ecs::Transform>(lock)) continue;
            auto &area = entity.Get<ecs::TriggerArea>(lock);
            auto areaTransform = entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock).GetTransform();
            glm::mat4 invAreaTransform = glm::inverse(glm::mat4(areaTransform));

            ecs::ComponentEvent<ecs::TriggerGroup> triggerEvent;
            while (triggerGroupObserver.Poll(lock, triggerEvent)) {
                if (triggerEvent.type == Tecs::EventType::REMOVED) {
                    area.contained_entities.erase(triggerEvent.entity);
                }
            }

            for (auto triggerEnt : lock.EntitiesWith<ecs::TriggerGroup>()) {
                if (!triggerEnt.Has<ecs::TriggerGroup, ecs::Transform>(lock)) continue;
                auto transform = triggerEnt.Get<ecs::Transform>(lock).GetGlobalTransform(lock);
                auto entityPos = glm::vec3(invAreaTransform * glm::vec4(transform.GetPosition(), 1.0));
                bool inArea = glm::all(glm::greaterThan(entityPos, glm::vec3(-0.5))) &&
                              glm::all(glm::lessThan(entityPos, glm::vec3(0.5)));

                if (area.contained_entities.contains(triggerEnt) == inArea) continue;
                if (inArea) {
                    area.contained_entities.insert(triggerEnt);
                    Debugf("Entity entered TriggerArea at: %f %f %f", entityPos.x, entityPos.y, entityPos.z);
                } else {
                    area.contained_entities.erase(triggerEnt);
                }
            }

            for (auto &triggerGroup : area.triggers) {
                for (auto &trigger : triggerGroup) {
                    std::visit(
                        [&lock, &area, &entity](auto &&arg) {
                            using T = std::decay_t<decltype(arg)>;
                            if constexpr (std::is_same<T, ecs::TriggerArea::TriggerCommand>()) {
                                for (auto &triggerEnt : area.contained_entities) {
                                    if (arg.executed_entities.contains(triggerEnt)) continue;

                                    Logf("TriggerArea running command: %s", arg.command);
                                    GetConsoleManager().QueueParseAndExecute(arg.command);
                                }
                                arg.executed_entities = area.contained_entities;
                            } else if constexpr (std::is_same<T, ecs::TriggerArea::TriggerSignal>()) {
                                if (entity.Has<ecs::SignalOutput>(lock)) {
                                    auto &signalOutput = entity.Get<ecs::SignalOutput>(lock);
                                    signalOutput.SetSignal(arg.outputSignal,
                                        area.contained_entities.size() > 0 ? 1.0 : 0.0);
                                }
                            }
                        },
                        trigger);
                }
            }
        }
    }
} // namespace sp
