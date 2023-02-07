#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>

namespace sp {
    class Gltf;
}

namespace physx {
    class PxJoint;
} // namespace physx

namespace ecs {
    enum class PhysicsJointType {
        // PhysX built-in joints
        Fixed = 0,
        Distance,
        Spherical,
        Hinge,
        Slider,

        // Custom joints
        Force, // Move actor0 to actor1 without exceeding force limits
        NoClip, // Prevents the 2 actors from colliding with eachother. Applies no forces.
        TemporaryNoClip, // Same as NoClip but removes itself once actors no longer overlap.
    };

    struct PhysicsJoint {
        EntityRef target;
        PhysicsJointType type = PhysicsJointType::Fixed;
        glm::vec2 limit = glm::vec2();
        Transform localOffset = Transform();
        Transform remoteOffset = Transform();

        bool operator==(const PhysicsJoint &) const = default;
    };

    static StructMetadata MetadataPhysicsJoint(typeid(PhysicsJoint),
        StructField::New("target", &PhysicsJoint::target),
        StructField::New("type", &PhysicsJoint::type),
        StructField::New("limit", &PhysicsJoint::limit),
        StructField::New("local_offset", &PhysicsJoint::localOffset),
        StructField::New("remote_offset", &PhysicsJoint::remoteOffset));

    struct PhysicsJoints {
        std::vector<PhysicsJoint> joints;

        void Add(const PhysicsJoint &joint) {
            if (!sp::contains(joints, joint)) joints.push_back(joint);
        }
    };

    static StructMetadata MetadataPhysicsJoints(typeid(PhysicsJoints),
        StructField::New(&PhysicsJoints::joints, FieldAction::AutoLoad));
    static Component<PhysicsJoints> ComponentPhysicsJoints("physics_joints", MetadataPhysicsJoints);

    template<>
    void StructMetadata::Save<PhysicsJoints>(const EntityScope &scope,
        picojson::value &dst,
        const PhysicsJoints &src,
        const PhysicsJoints &def);
    template<>
    void Component<PhysicsJoints>::Apply(PhysicsJoints &dst, const PhysicsJoints &src, bool liveTarget);
} // namespace ecs
