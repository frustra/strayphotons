/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>
#include <optional>

namespace ecs {
    struct PhysicsQuery {
        template<typename T>
        struct Handle {
            size_t index = ~0u;

            explicit operator bool() const {
                return index != ~0u;
            }
        };

        struct Raycast {
            float maxDistance;
            PhysicsGroupMask filterGroup;
            glm::vec3 direction = {0, 0, -1};
            bool relativeDirection = true;

            glm::vec3 position = {0, 0, 0};
            bool relativePosition = true;
            uint32 maxHits = 1;

            bool operator==(const Raycast &other) const {
                return filterGroup == other.filterGroup && maxDistance == other.maxDistance &&
                       direction == other.direction && position == other.position &&
                       relativeDirection == other.relativeDirection && relativePosition == other.relativePosition &&
                       maxHits == other.maxHits;
            }

            struct Result {
                Entity target, subTarget;
                glm::vec3 position, normal;
                float distance;
                uint32 hits;
            };
            std::optional<Result> result;

            Raycast() : maxDistance(0), filterGroup((PhysicsGroupMask)0) {}
            Raycast(float maxDistance, PhysicsGroupMask filterGroup = PhysicsGroupMask(PHYSICS_GROUP_WORLD))
                : maxDistance(maxDistance), filterGroup(filterGroup) {}
        };

        struct Sweep {
            PhysicsShape shape;
            PhysicsGroupMask filterGroup;
            glm::vec3 sweepDirection;
            float maxDistance;

            bool operator==(const Sweep &other) const {
                return shape == other.shape && filterGroup == other.filterGroup &&
                       sweepDirection == other.sweepDirection && maxDistance == other.maxDistance;
            }

            struct Result {
                Entity target;
                glm::vec3 position;
                float distance;
            };
            std::optional<Result> result;

            Sweep() : shape(), maxDistance(0), filterGroup((PhysicsGroupMask)0), sweepDirection(0) {}
            Sweep(const PhysicsShape &shape,
                float maxDistance,
                PhysicsGroupMask filterGroup = PhysicsGroupMask(PHYSICS_GROUP_WORLD),
                glm::vec3 sweepDirection = glm::vec3(0, 0, -1))
                : shape(shape), filterGroup(filterGroup), sweepDirection(sweepDirection), maxDistance(maxDistance) {}
        };

        struct Overlap {
            PhysicsShape shape;
            PhysicsGroupMask filterGroup;

            bool operator==(const Overlap &other) const {
                return shape == other.shape && filterGroup == other.filterGroup;
            }

            std::optional<Entity> result;

            Overlap() : shape(), filterGroup((PhysicsGroupMask)0) {}
            Overlap(const PhysicsShape &shape, PhysicsGroupMask filterGroup = PhysicsGroupMask(PHYSICS_GROUP_WORLD))
                : shape(shape), filterGroup(filterGroup) {}
        };

        struct Mass {
            EntityRef targetActor;

            bool operator==(const Mass &other) const {
                return targetActor == other.targetActor;
            }

            struct Result {
                float weight;
                glm::vec3 centerOfMass;
            };
            std::optional<Result> result;

            Mass() : targetActor() {}
            Mass(const EntityRef &targetActor) : targetActor(targetActor) {}
        };

        std::vector<std::variant<std::monostate, Raycast, Sweep, Overlap, Mass>> queries;

        // Calling NewQuery() invalidates all references returned by Lookup()
        template<typename T>
        Handle<T> NewQuery(const T &query) {
            for (size_t i = 0; i < queries.size(); i++) {
                if (std::holds_alternative<std::monostate>(queries[i])) {
                    queries[i] = query;
                    return {i};
                }
            }
            queries.emplace_back(query);
            return {queries.size() - 1};
        }

        // Calling NewQuery() invalidates all references returned by Lookup()
        template<typename T>
        T &Lookup(const Handle<T> &handle) {
            static T invalidResult = {};
            if (handle.index >= queries.size()) {
                Warnf("Reading invalid query handle");
                return invalidResult;
            }
            return std::get<T>(queries[handle.index]);
        }

        template<typename T>
        void RemoveQuery(Handle<T> &handle) {
            if (handle.index < queries.size()) queries[handle.index] = std::monostate();
            handle = {};
        }
    };

    static Component<PhysicsQuery> ComponentPhysicsQuery({typeid(PhysicsQuery), "physics_query", ""});
} // namespace ecs
