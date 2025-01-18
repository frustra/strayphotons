/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
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
    static const StructMetadata MetadataPhysicsGroup(typeid(PhysicsGroup),
        "PhysicsGroup",
        "An actor's physics group determines both what it will collide with in the physics simulation, "
        "and which physics queries it is visible to.",
        StructMetadata::EnumDescriptions{
            {(uint32_t)PhysicsGroup::NoClip, "Actors in this collision group will not collide with anything."},
            {(uint32_t)PhysicsGroup::World,
                "This is the default collision group. All actors in this group will collide with eachother."},
            {(uint32_t)PhysicsGroup::Interactive,
                "This group behaves like `World` but allows behavior to be customized for movable objects."},
            {(uint32_t)PhysicsGroup::HeldObject,
                "Held objects do not collide with the player, "
                "but will collide with other held objects and the rest of the world."},
            {(uint32_t)PhysicsGroup::Player,
                "This group is for the player's body, which collides with the world, "
                "but not other objects in any of the player groups."},
            {(uint32_t)PhysicsGroup::PlayerLeftHand,
                "The player's left hand collides with the right hand, but not itself or the player's body."},
            {(uint32_t)PhysicsGroup::PlayerRightHand,
                "The player's right hand collides with the left hand, but not itself or the player's body."},
            {(uint32_t)PhysicsGroup::UserInterface,
                "This collision group is for popup UI elements that will only collide with the player's hands."},
        });

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

    static StructMetadata MetadataPhysicsShape(typeid(PhysicsShape),
        "PhysicsShape",
        R"(
Most physics shapes correlate with the underlying [PhysX Geometry Shapes](https://gameworksdocs.nvidia.com/PhysX/4.1/documentation/physxguide/Manual/Geometry.html).
The diagrams provided in the PhysX docs may be helpful in visualizing collisions.
Additionally an in-engine debug overlay can be turned on by entering `x.DebugColliders 1` in the consle.

A shape type is defined by setting one of the following additional fields:
| Shape Field | Type    | Default Value   | Description |
|-------------|---------|-----------------|-------------|
| **model**   | string  | ""              | Name of the cooked physics collision mesh to load |
| **plane**   | Plane   | {}              | Planes always face the +X axis relative to the actor |
| **capsule** | Capsule | {"radius": 0.5, "height": 1.0} | A capsule's total length along the X axis will be equal to `height + radius * 2` |
| **sphere**  | float   | 1.0             | Spheres are defined by their radius |
| **box**     | vec3    | [1.0, 1.0, 1.0] | Boxes define their dimensions by specifying the total length along the X, Y, and Z axes relative to the actor |

GLTF models automatically generate convex hull collision meshes.
They can be referenced by name in the form:  
`"<model_name>.convex<mesh_index>"`
e.g. `"box.convex0"`

If only a model name is specified, `convex0` will be used by default.

If a `model_name.physics.json` file is provided alongside the GLTF, then custom physics meshes can be generated and configured.
For example, the `duck.physics.json` physics definition defines `"duck.cooked"`,
which decomposes the duck model into multiple convex hulls to more accurately represent its non-convex shape.
)",
        StructField::New("transform",
            "The position and orientation of the shape relative to the actor's origin (the entity transform position)",
            &PhysicsShape::transform,
            FieldAction::None),
        StructField::New(&PhysicsShape::material, FieldAction::None));
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
    static const StructMetadata MetadataPhysicsActorType(typeid(PhysicsActorType),
        "PhysicsActorType",
        "A physics actor's type determines how it behaves in the world. The type should match the intended usage of an "
        "object. Dynamic actor's positions are taken over by the physics system, but scripts may still control these "
        "actors with physics joints or force-based constraints.",
        StructMetadata::EnumDescriptions{
            {(uint32_t)PhysicsActorType::Static,
                "The physics actor will not move. Used for walls, floors, and other static objects."},
            {(uint32_t)PhysicsActorType::Dynamic, "The physics actor has a mass and is affected by gravity."},
            {(uint32_t)PhysicsActorType::Kinematic,
                "The physics actor has infinite mass and is controlled by script or animation."},
            {(uint32_t)PhysicsActorType::SubActor,
                "The shapes defined on this virtual physics actor are added to the parent physics actor instead."},
        });

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

        glm::vec3 constantForce = glm::vec3(0);
    };

    static Component<Physics> ComponentPhysics({typeid(Physics),
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
            &Physics::constantForce)});
} // namespace ecs
