#include "TriggerSystem.hh"

#include "console/Console.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
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
        ZoneScoped;
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::TriggerGroup, ecs::Transform>,
            ecs::Write<ecs::TriggerArea, ecs::SignalOutput>>();

        for (auto &entity : lock.EntitiesWith<ecs::TriggerArea>()) {
            if (!entity.Has<ecs::TriggerArea, ecs::Transform>(lock)) continue;
            auto &area = entity.Get<ecs::TriggerArea>(lock);
            auto areaTransform = entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock).GetMatrix();
            glm::mat4 invAreaTransform = glm::inverse(glm::mat4(areaTransform));

            ecs::ComponentEvent<ecs::TriggerGroup> triggerEvent;
            while (triggerGroupObserver.Poll(lock, triggerEvent)) {
                if (triggerEvent.type == Tecs::EventType::REMOVED) {
                    for (auto &triggerGroup : area.triggers) {
                        for (auto &trigger : triggerGroup) {
                            std::visit(
                                [&triggerEvent](auto &&arg) {
                                    arg.contained_entities.erase(triggerEvent.entity);
                                },
                                trigger);
                        }
                    }
                }
            }

            for (auto triggerEnt : lock.EntitiesWith<ecs::TriggerGroup>()) {
                if (!triggerEnt.Has<ecs::TriggerGroup, ecs::Transform>(lock)) continue;
                auto transform = triggerEnt.Get<ecs::Transform>(lock).GetGlobalTransform(lock);
                auto entityPos = glm::vec3(invAreaTransform * glm::vec4(transform.GetPosition(), 1.0));
                bool inArea = glm::all(glm::greaterThan(entityPos, glm::vec3(-0.5))) &&
                              glm::all(glm::lessThan(entityPos, glm::vec3(0.5)));

                auto &triggerGroup = triggerEnt.Get<ecs::TriggerGroup>(lock);
                for (auto &trigger : area.triggers[triggerGroup]) {
                    std::visit(
                        [&lock, &entity, &triggerEnt, &entityPos, &inArea](auto &&arg) {
                            if (arg.contained_entities.contains(triggerEnt) == inArea) return;
                            if (inArea) {
                                arg.contained_entities.insert(triggerEnt);
                                Debugf("%s entered TriggerArea %s at: %f %f %f",
                                    ecs::ToString(lock, triggerEnt),
                                    ecs::ToString(lock, entity),
                                    entityPos.x,
                                    entityPos.y,
                                    entityPos.z);

                                using T = std::decay_t<decltype(arg)>;
                                if constexpr (std::is_same<T, ecs::TriggerArea::TriggerCommand>()) {
                                    Logf("TriggerArea running command: %s", arg.command);
                                    GetConsoleManager().QueueParseAndExecute(arg.command);
                                }
                            } else {
                                arg.contained_entities.erase(triggerEnt);
                            }
                        },
                        trigger);
                }
            }

            if (entity.Has<ecs::SignalOutput>(lock)) {
                for (auto &triggerGroup : area.triggers) {
                    for (auto &trigger : triggerGroup) {
                        std::visit(
                            [&lock, &entity](auto &&arg) {
                                using T = std::decay_t<decltype(arg)>;
                                if constexpr (std::is_same<T, ecs::TriggerArea::TriggerSignal>()) {
                                    auto &signalOutput = entity.Get<ecs::SignalOutput>(lock);
                                    signalOutput.SetSignal(arg.outputSignal,
                                        arg.contained_entities.size() > 0 ? 1.0 : 0.0);
                                }
                            },
                            trigger);
                    }
                }
            }
        }
    }
} // namespace sp
