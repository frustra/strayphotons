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
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::TriggerGroup, ecs::TransformSnapshot>,
            ecs::Write<ecs::TriggerArea, ecs::SignalOutput>,
            ecs::SendEventsLock>();

        for (auto &entity : lock.EntitiesWith<ecs::TriggerArea>()) {
            if (!entity.Has<ecs::TriggerArea, ecs::TransformSnapshot>(lock)) continue;
            auto &area = entity.Get<ecs::TriggerArea>(lock);
            auto invAreaTransform = entity.Get<ecs::TransformSnapshot>(lock).GetInverse();

            ecs::ComponentEvent<ecs::TriggerGroup> triggerEvent;
            while (triggerGroupObserver.Poll(lock, triggerEvent)) {
                if (triggerEvent.type == Tecs::EventType::REMOVED) {
                    for (auto &containedEntities : area.containedEntities) {
                        containedEntities.erase(triggerEvent.entity);
                    }
                }
            }

            for (auto triggerEnt : lock.EntitiesWith<ecs::TriggerGroup>()) {
                if (!triggerEnt.Has<ecs::TriggerGroup, ecs::TransformSnapshot>(lock)) continue;
                auto &transform = triggerEnt.Get<ecs::TransformSnapshot>(lock);
                auto entityPos = invAreaTransform * glm::vec4(transform.GetPosition(), 1.0);
                bool inArea = glm::all(glm::greaterThan(entityPos, glm::vec3(-0.5))) &&
                              glm::all(glm::lessThan(entityPos, glm::vec3(0.5)));

                auto &triggerGroup = triggerEnt.Get<ecs::TriggerGroup>(lock);
                auto &containedEntities = area.containedEntities[triggerGroup];
                if (containedEntities.contains(triggerEnt) == inArea) continue;

                std::string *eventName;

                if (inArea) {
                    eventName = &ecs::TriggerGroupEventNames[triggerGroup].first;
                    containedEntities.insert(triggerEnt);
                    Debugf("%s entered TriggerArea %s at: %f %f %f",
                        ecs::ToString(lock, triggerEnt),
                        ecs::ToString(lock, entity),
                        entityPos.x,
                        entityPos.y,
                        entityPos.z);
                } else {
                    eventName = &ecs::TriggerGroupEventNames[triggerGroup].second;
                    containedEntities.erase(triggerEnt);
                }

                ecs::EventBindings::SendEvent(lock, *eventName, entity, triggerEnt);
            }

            for (size_t i = 0; i < (size_t)ecs::TriggerGroup::Count; i++) {
                auto triggerGroup = (ecs::TriggerGroup)i;
                auto entityCount = area.containedEntities[triggerGroup].size();
                if (entity.Has<ecs::SignalOutput>(lock)) {
                    auto &signalOutput = entity.Get<ecs::SignalOutput>(lock);
                    signalOutput.SetSignal(ecs::TriggerGroupSignalNames[triggerGroup], (double)entityCount);
                }
            }
        }
    }
} // namespace sp
