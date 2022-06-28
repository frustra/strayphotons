#pragma once

#include "assets/Async.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>
#include <memory>

namespace sp {
    class Gltf;
}

namespace ecs {
    enum class PhysicsGroup : uint16_t {
        NoClip = 0,
        World,
        WorldOverlap,
        Interactive,
        HeldObject,
        Player,
        PlayerLeftHand,
        PlayerRightHand,
        UserInterface,
        Count,
    };

    enum PhysicsGroupMask {
        PHYSICS_GROUP_NOCLIP = 1 << (size_t)PhysicsGroup::NoClip,
        PHYSICS_GROUP_WORLD = 1 << (size_t)PhysicsGroup::World,
        PHYSICS_GROUP_WORLD_OVERLAP = 1 << (size_t)PhysicsGroup::WorldOverlap,
        PHYSICS_GROUP_INTERACTIVE = 1 << (size_t)PhysicsGroup::Interactive,
        PHYSICS_GROUP_HELD_OBJECT = 1 << (size_t)PhysicsGroup::HeldObject,
        PHYSICS_GROUP_PLAYER = 1 << (size_t)PhysicsGroup::Player,
        PHYSICS_GROUP_PLAYER_LEFT_HAND = 1 << (size_t)PhysicsGroup::PlayerLeftHand,
        PHYSICS_GROUP_PLAYER_RIGHT_HAND = 1 << (size_t)PhysicsGroup::PlayerRightHand,
        PHYSICS_GROUP_USER_INTERFACE = 1 << (size_t)PhysicsGroup::UserInterface,
    };

    struct PhysicsShape {
        struct Sphere {
            float radius;
            Sphere(float radius = 1.0f) : radius(radius) {}

            bool operator==(const Sphere &other) const {
                return glm::epsilonEqual(radius, other.radius, 1e-5f);
            }
        };

        struct Capsule {
            float radius;
            float height;
            Capsule(float height = 1.0f, float radius = 0.5f) : radius(radius), height(height) {}

            bool operator==(const Capsule &other) const {
                return glm::epsilonEqual(radius, other.radius, 1e-5f) && glm::epsilonEqual(height, other.height, 1e-5f);
            }
        };

        struct Box {
            glm::vec3 extents;
            Box(glm::vec3 extents = glm::vec3(1)) : extents(extents) {}

            bool operator==(const Box &other) const {
                return glm::all(glm::epsilonEqual(extents, other.extents, 1e-5f));
            }
        };

        struct Plane {
            bool operator==(const Plane &) const = default;
        };

        struct ConvexMesh {
            sp::AsyncPtr<sp::Gltf> model;
            size_t meshIndex = 0;
            bool decomposeHull = false;

            ConvexMesh() {}
            ConvexMesh(sp::AsyncPtr<sp::Gltf> model, size_t meshIndex = 0, bool decomposeHull = false)
                : model(model), meshIndex(meshIndex), decomposeHull(decomposeHull) {}

            bool operator==(const ConvexMesh &) const = default;
        };

        std::variant<std::monostate, Sphere, Capsule, Box, Plane, ConvexMesh> shape;
        Transform transform;

        PhysicsShape() : shape(std::monostate()) {}
        PhysicsShape(Sphere sphere, Transform transform = Transform()) : shape(sphere), transform(transform) {}
        PhysicsShape(Capsule capsule, Transform transform = Transform()) : shape(capsule), transform(transform) {}
        PhysicsShape(Box box, Transform transform = Transform()) : shape(box), transform(transform) {}
        PhysicsShape(Plane plane, Transform transform = Transform()) : shape(plane), transform(transform) {}
        PhysicsShape(ConvexMesh mesh) : shape(mesh) {}
        PhysicsShape(sp::AsyncPtr<sp::Gltf> model, size_t meshIndex = 0) : shape(ConvexMesh(model, meshIndex)) {}

        operator bool() const {
            return !std::holds_alternative<std::monostate>(shape);
        }

        bool operator==(const PhysicsShape &) const = default;
    };

    struct Physics {
        Physics() {}
        Physics(PhysicsShape shape, PhysicsGroup group = PhysicsGroup::World, bool dynamic = true, float density = 1.0f)
            : shapes({shape}), group(group), dynamic(dynamic), density(density) {}

        std::vector<PhysicsShape> shapes;

        PhysicsGroup group = PhysicsGroup::World;
        bool dynamic = true;
        bool kinematic = false; // only dynamic actors can be kinematic
        float density = 1000.0f; // kg/m^3
        float angularDamping = 0.05f;
        float linearDamping = 0.0f;

        glm::vec3 constantForce;

        EntityRef constraint;
        float constraintMaxDistance = 0.0f;
        glm::vec3 constraintOffset;
        glm::quat constraintRotation;

        void SetConstraint(EntityRef target,
            float maxDistance = 0.0f,
            glm::vec3 offset = glm::vec3(),
            glm::quat rotation = glm::quat()) {
            constraint = target;
            constraintMaxDistance = maxDistance;
            constraintOffset = offset;
            constraintRotation = rotation;
        }

        void RemoveConstraint() {
            SetConstraint(Entity());
        }
    };

    static Component<Physics> ComponentPhysics("physics");

    template<>
    bool Component<Physics>::Load(const EntityScope &scope, Physics &dst, const picojson::value &src);
} // namespace ecs
