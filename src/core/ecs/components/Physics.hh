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
        StructField::New("static_friction", &PhysicsMaterial::staticFriction),
        StructField::New("dynamic_friction", &PhysicsMaterial::dynamicFriction),
        StructField::New("restitution", &PhysicsMaterial::restitution));

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

    static StructMetadata MetadataPhysicsShape(typeid(PhysicsShape));
    template<>
    bool StructMetadata::Load<PhysicsShape>(const EntityScope &scope, PhysicsShape &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<PhysicsShape>(const EntityScope &scope,
        picojson::value &dst,
        const PhysicsShape &src,
        const PhysicsShape &def);

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
        StructField::New("shapes", &Physics::shapes),
        StructField::New("group", &Physics::group),
        StructField::New("type", &Physics::type),
        StructField::New("parent_actor", &Physics::parentActor),
        StructField::New("mass", &Physics::mass),
        StructField::New("density", &Physics::density),
        StructField::New("angular_damping", &Physics::angularDamping),
        StructField::New("linear_damping", &Physics::linearDamping),
        StructField::New("contact_report_force", &Physics::contactReportThreshold),
        StructField::New("force", &Physics::constantForce));
    static Component<Physics> ComponentPhysics("physics", MetadataPhysics);
} // namespace ecs
