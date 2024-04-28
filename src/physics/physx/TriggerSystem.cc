/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "TriggerSystem.hh"

#include "common/Common.hh"
#include "common/Logging.hh"
#include "common/Tracing.hh"
#include "console/Console.hh"
#include "ecs/EcsImpl.hh"

#include <glm/gtx/norm.hpp>

namespace sp {
    TriggerSystem::TriggerSystem() {
        auto lock = ecs::StartTransaction<ecs::AddRemove>();
        triggerGroupObserver = lock.Watch<ecs::ComponentEvent<ecs::TriggerGroup>>();
    }

    TriggerSystem::~TriggerSystem() {
        auto lock = ecs::StartTransaction<ecs::AddRemove>();
        triggerGroupObserver.Stop(lock);
    }

    void TriggerSystem::Frame(ecs::Lock<ecs::Read<ecs::Name, ecs::TriggerGroup, ecs::TransformSnapshot>,
        ecs::Write<ecs::TriggerArea, ecs::Signals>,
        ecs::SendEventsLock> lock) {
        ZoneScoped;

        for (auto &entity : lock.EntitiesWith<ecs::TriggerArea>()) {
            if (!entity.Has<ecs::TriggerArea, ecs::TransformSnapshot>(lock)) continue;
            auto &area = entity.Get<ecs::TriggerArea>(lock);
            auto &areaTransform = entity.Get<ecs::TransformSnapshot>(lock).globalPose;
            auto areaCenter = areaTransform.GetPosition();
            auto boundingRadiusSquared = glm::length2(areaTransform * glm::vec4(glm::vec3(0.5f), 0.0f));
            auto invAreaTransform = areaTransform.GetInverse();

            ecs::ComponentEvent<ecs::TriggerGroup> triggerEvent;
            while (triggerGroupObserver.Poll(lock, triggerEvent)) {
                if (triggerEvent.type == Tecs::EventType::REMOVED) {
                    for (auto &containedEntities : area.containedEntities) {
                        containedEntities.erase(triggerEvent.entity);
                    }
                }
            }

            for (auto &triggerEnt : lock.EntitiesWith<ecs::TriggerGroup>()) {
                if (!triggerEnt.Has<ecs::TriggerGroup, ecs::TransformSnapshot>(lock)) continue;
                auto &transform = triggerEnt.Get<ecs::TransformSnapshot>(lock).globalPose;
                auto entityPos = transform.GetPosition();
                bool inArea = false;
                if (glm::length2(entityPos - areaCenter) <= boundingRadiusSquared) {
                    auto relativePos = invAreaTransform * glm::vec4(transform.GetPosition(), 1.0);
                    switch (area.shape) {
                    case ecs::TriggerShape::Box:
                        inArea = glm::all(glm::greaterThan(relativePos, glm::vec3(-0.5))) &&
                                 glm::all(glm::lessThan(relativePos, glm::vec3(0.5)));
                        break;
                    case ecs::TriggerShape::Sphere:
                        inArea = glm::length2(relativePos) < 0.25;
                        break;
                    }
                }

                auto &triggerGroup = triggerEnt.Get<ecs::TriggerGroup>(lock);
                auto &containedEntities = area.containedEntities[triggerGroup];
                if (containedEntities.contains(triggerEnt) == inArea) continue;

                std::string *eventName;

                if (inArea) {
                    eventName = &ecs::TriggerGroupEventNames[triggerGroup].first;
                    containedEntities.insert(triggerEnt);
                    Tracef("%s entered TriggerArea %s at: %f %f %f",
                        ecs::ToString(lock, triggerEnt),
                        ecs::ToString(lock, entity),
                        entityPos.x,
                        entityPos.y,
                        entityPos.z);
                } else {
                    eventName = &ecs::TriggerGroupEventNames[triggerGroup].second;
                    containedEntities.erase(triggerEnt);
                    Tracef("%s leaving TriggerArea %s at: %f %f %f",
                        ecs::ToString(lock, triggerEnt),
                        ecs::ToString(lock, entity),
                        entityPos.x,
                        entityPos.y,
                        entityPos.z);
                }

                ecs::EventBindings::SendEvent(lock, entity, ecs::Event{*eventName, entity, triggerEnt});
            }

            for (auto &triggerGroup : magic_enum::enum_values<ecs::TriggerGroup>()) {
                auto entityCount = area.containedEntities[triggerGroup].size();
                ecs::SignalRef(entity, ecs::TriggerGroupSignalNames[triggerGroup]).SetValue(lock, (double)entityCount);
            }
        }
    }
} // namespace sp
