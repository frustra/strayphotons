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
        triggerGroupObserver = lock.Watch<ecs::ComponentAddRemoveEvent<ecs::TriggerGroup>>();
    }

    TriggerSystem::~TriggerSystem() {
        auto lock = ecs::StartTransaction<ecs::AddRemove>();
        triggerGroupObserver.Stop(lock);
    }

    void TriggerSystem::Frame(ecs::Lock<ecs::Read<ecs::Name, ecs::TriggerGroup, ecs::TransformSnapshot>,
        ecs::Write<ecs::TriggerArea, ecs::Signals>,
        ecs::SendEventsLock> lock) {
        ZoneScoped;

        ecs::ComponentAddRemoveEvent<ecs::TriggerGroup> triggerEvent;
        while (triggerGroupObserver.Poll(lock, triggerEvent)) {
            if (triggerEvent.type == Tecs::EventType::REMOVED) {
                for (auto &entity : lock.EntitiesWith<ecs::TriggerArea>()) {
                    auto &containedEntities = entity.Get<ecs::TriggerArea>(lock).containedEntities;
                    containedEntities[triggerEvent.component].erase(triggerEvent.entity);
                }
            }
        }
    }

    void TriggerSystem::UpdateEntityTriggers(ecs::Lock<ecs::Read<ecs::Name, ecs::TriggerGroup, ecs::TransformSnapshot>,
                                                 ecs::Write<ecs::TriggerArea, ecs::Signals>,
                                                 ecs::SendEventsLock> lock,
        ecs::Entity entity) {
        if (!entity.Has<ecs::TriggerGroup, ecs::TransformSnapshot>(lock)) return;
        ZoneScoped;

        auto &triggerGroup = entity.Get<ecs::TriggerGroup>(lock);
        auto &transform = entity.Get<ecs::TransformSnapshot>(lock).globalPose;
        auto entityPos = transform.GetPosition();

        for (auto &areaEnt : lock.EntitiesWith<ecs::TriggerArea>()) {
            if (!areaEnt.Has<ecs::TriggerArea, ecs::TransformSnapshot>(lock)) continue;
            auto &area = areaEnt.Get<ecs::TriggerArea>(lock);
            auto &areaTransform = areaEnt.Get<ecs::TransformSnapshot>(lock).globalPose;

            auto boundingRadiusSquared = glm::length2(areaTransform * glm::vec4(glm::vec3(0.5f), 0.0f));
            bool inArea = false;
            if (glm::length2(entityPos - areaTransform.GetPosition()) <= boundingRadiusSquared) {
                auto relativePos = areaTransform.GetInverse() * glm::vec4(transform.GetPosition(), 1.0);
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

            auto &containedEntities = area.containedEntities[triggerGroup];
            if (containedEntities.contains(entity) != inArea) {
                std::string *eventName;

                if (inArea) {
                    eventName = &ecs::TriggerGroupEventNames[triggerGroup].first;
                    containedEntities.insert(entity);
                    Tracef("%s entered TriggerArea %s at: %f %f %f",
                        ecs::ToString(lock, entity),
                        ecs::ToString(lock, areaEnt),
                        entityPos.x,
                        entityPos.y,
                        entityPos.z);
                } else {
                    eventName = &ecs::TriggerGroupEventNames[triggerGroup].second;
                    containedEntities.erase(entity);
                    Tracef("%s leaving TriggerArea %s at: %f %f %f",
                        ecs::ToString(lock, entity),
                        ecs::ToString(lock, entity),
                        entityPos.x,
                        entityPos.y,
                        entityPos.z);
                }

                ecs::EventBindings::SendEvent(lock, areaEnt, ecs::Event{*eventName, areaEnt, entity});

                ecs::SignalRef(areaEnt, ecs::TriggerGroupSignalNames[triggerGroup])
                    .SetValue(lock, (double)containedEntities.size());
            }
        }
    }
} // namespace sp
