#pragma once

#include "ecs/Components.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>
#include <optional>

namespace ecs {
    struct PhysicsQuery {
        struct Raycast {
            float maxDistance;
            PhysicsGroupMask filterGroup;
            Transform rayOffset;

            bool operator==(const Raycast &other) const {
                return filterGroup == other.filterGroup && maxDistance == other.maxDistance && rayOffset == rayOffset;
            }

            struct Result {
                Entity target;
                glm::vec3 position;
                float distance;
            };
            std::optional<Result> result;

            Raycast(float maxDistance,
                PhysicsGroupMask filterGroup = PHYSICS_GROUP_WORLD,
                Transform rayOffset = Transform())
                : maxDistance(maxDistance), filterGroup(filterGroup), rayOffset(rayOffset) {}
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

            Sweep(const PhysicsShape &shape,
                float maxDistance,
                PhysicsGroupMask filterGroup = PHYSICS_GROUP_WORLD,
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

            Overlap(const PhysicsShape &shape, PhysicsGroupMask filterGroup = PHYSICS_GROUP_WORLD)
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

            Mass(const EntityRef &targetActor) : targetActor(targetActor) {}
        };

        std::vector<std::variant<Raycast, Sweep, Overlap, Mass>> queries;

        template<typename T>
        bool ReadQuery(T &searchQuery) const {
            for (auto &query : queries) {
                auto *variant = std::get_if<T>(&query);
                if (*variant == searchQuery) {
                    searchQuery.result = variant->result;
                    return true;
                }
            }
            return false;
        }
    };

    static Component<PhysicsQuery> ComponentPhysicsQuery("physics_query");

    template<>
    bool Component<PhysicsQuery>::Load(const EntityScope &scope, PhysicsQuery &dst, const picojson::value &src);
} // namespace ecs
