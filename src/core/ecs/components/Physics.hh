#pragma once

#include "assets/Async.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>
#include <memory>
#include <variant>

namespace sp {
    class Gltf;
    struct HullSettings;
} // namespace sp

namespace ecs {
    enum class PhysicsGroup : uint16_t {
        NoClip = 0,
        World,
        Interactive,
        HeldObject,
        Player,
        PlayerLeftHand,
        PlayerRightHand,
        UserInterface,
    };

    enum PhysicsGroupMask {
        PHYSICS_GROUP_NOCLIP = 1 << (size_t)PhysicsGroup::NoClip,
        PHYSICS_GROUP_WORLD = 1 << (size_t)PhysicsGroup::World,
        PHYSICS_GROUP_INTERACTIVE = 1 << (size_t)PhysicsGroup::Interactive,
        PHYSICS_GROUP_HELD_OBJECT = 1 << (size_t)PhysicsGroup::HeldObject,
        PHYSICS_GROUP_PLAYER = 1 << (size_t)PhysicsGroup::Player,
        PHYSICS_GROUP_PLAYER_LEFT_HAND = 1 << (size_t)PhysicsGroup::PlayerLeftHand,
        PHYSICS_GROUP_PLAYER_RIGHT_HAND = 1 << (size_t)PhysicsGroup::PlayerRightHand,
        PHYSICS_GROUP_USER_INTERFACE = 1 << (size_t)PhysicsGroup::UserInterface,
    };

    static_assert(magic_enum::enum_count<PhysicsGroup>() <= sizeof(uint32_t) * 8, "Too many PhysicsGroups defined");

    struct PhysicsMaterial {
        float staticFriction = 0.6f;
        float dynamicFriction = 0.5f;
        float restitution = 0.0f;

        bool operator==(const PhysicsMaterial &) const = default;
    };
    static StructMetadata MetadataPhysicsMaterial(typeid(PhysicsMaterial),
        "PhysicsMaterial",
        "",
        StructField::New("static_friction",
            "This material's coefficient of static friction (>= 0.0)",
            &PhysicsMaterial::staticFriction),
        StructField::New("dynamic_friction",
            "This material's coefficient of dynamic friction (>= 0.0)",
            &PhysicsMaterial::dynamicFriction),
        StructField::New("restitution",
            "This material's coefficient of restitution (0.0 no bounce - 1.0 more bounce)",
            &PhysicsMaterial::restitution));

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
            std::string modelName, meshName;
            sp::AsyncPtr<sp::Gltf> model;
            sp::AsyncPtr<sp::HullSettings> hullSettings;

            ConvexMesh() {}
            ConvexMesh(const std::string &fullMeshName);
            ConvexMesh(const std::string &modelName, const std::string &meshName);
            ConvexMesh(const std::string &modelName, size_t meshIndex)
                : ConvexMesh(modelName, "convex" + std::to_string(meshIndex)) {}

            bool operator==(const ConvexMesh &) const = default;
        };

        std::variant<std::monostate, Sphere, Capsule, Box, Plane, ConvexMesh> shape;
        Transform transform;
        PhysicsMaterial material;

        PhysicsShape() : shape(std::monostate()) {}
        PhysicsShape(Sphere sphere, Transform transform = Transform()) : shape(sphere), transform(transform) {}
        PhysicsShape(Capsule capsule, Transform transform = Transform()) : shape(capsule), transform(transform) {}
        PhysicsShape(Box box, Transform transform = Transform()) : shape(box), transform(transform) {}
        PhysicsShape(Plane plane, Transform transform = Transform()) : shape(plane), transform(transform) {}
        PhysicsShape(ConvexMesh mesh, Transform transform = Transform()) : shape(mesh), transform(transform) {}
        PhysicsShape(const std::string &fullMeshName) : shape(ConvexMesh(fullMeshName)), transform() {}

        explicit operator bool() const {
            return !std::holds_alternative<std::monostate>(shape);
        }

        bool operator==(const PhysicsShape &) const = default;
    };

    static StructMetadata MetadataPhysicsShape(typeid(PhysicsShape), "PhysicsShape", "");
    template<>
    bool StructMetadata::Load<PhysicsShape>(PhysicsShape &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<PhysicsShape>(const EntityScope &scope,
        picojson::value &dst,
        const PhysicsShape &src,
        const PhysicsShape *def);

    enum class PhysicsActorType : uint8_t {
        Static,
        Dynamic,
        Kinematic,
        SubActor,
    };

    struct Physics {
        Physics() {}
        Physics(PhysicsShape shape,
            PhysicsGroup group = PhysicsGroup::World,
            PhysicsActorType type = PhysicsActorType::Dynamic,
            float mass = 1.0f)
            : shapes({shape}), group(group), type(type), mass(mass) {}

        std::vector<PhysicsShape> shapes;

        PhysicsGroup group = PhysicsGroup::World;
        PhysicsActorType type = PhysicsActorType::Dynamic;
        EntityRef parentActor;
        float mass = 0.0f; // kilograms (density used if mass is zero)
        float density = 1000.0f; // kg/m^3
        float angularDamping = 0.05f;
        float linearDamping = 0.0f;
        float contactReportThreshold = -1.0f;

        glm::vec3 constantForce;
    };

    static StructMetadata MetadataPhysics(typeid(Physics),
        "physics",
        "",
        StructField::New("shapes",
            "A list of individual shapes and models that combine to form the actor's overall collision shape.",
            &Physics::shapes),
        StructField::New("group", "The collision group that this actor belongs to.", &Physics::group),
        StructField::New("type",
            "\"Dynamic\" objects are affected by gravity, while Kinematic objects have an infinite mass and are only "
            "movable by game logic. \"Static\" objects are meant to be immovable and will not push objects if moved. "
            "The \"SubActor\" type adds this entity's shapes to the **parent_actor** entity instead of creating a new "
            "physics actor.",
            &Physics::type),
        StructField::New("parent_actor",
            "Only used for \"SubActor\" type. If empty, the parent actor is determined by the `transform` parent.",
            &Physics::parentActor),
        StructField::New("mass",
            "The weight of the physics actor in Kilograms (kg). Overrides **density** field. "
            "Only used for \"Dynamic\" objects.",
            &Physics::mass),
        StructField::New("density",
            "The density of the physics actor in Kilograms per Cubic Meter (kg/m^3). "
            "This value is ignored if **mass** != 0. Only used for \"Dynamic\" objects.",
            &Physics::density),
        StructField::New("angular_damping",
            "Resistance to changes in rotational velocity. Affects how quickly the entity will stop spinning. (>= 0.0)",
            &Physics::angularDamping),
        StructField::New("linear_damping",
            "Resistance to changes in linear velocity. Affects how quickly the entity will stop moving. (>= 0.0)",
            &Physics::linearDamping),
        StructField::New("contact_report_force",
            "The minimum collision force required to trigger a contact event. "
            "Force-based contact events are enabled if this value is >= 0.0",
            &Physics::contactReportThreshold),
        StructField::New("constant_force",
            "A vector defining a constant force (in Newtons, N) that should be applied to the actor. "
            "The force vector is applied relative to the actor at its center of mass.",
            &Physics::constantForce));
    static Component<Physics> ComponentPhysics(MetadataPhysics);
} // namespace ecs
