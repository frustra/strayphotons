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

        // Custom force-limited joints
        Force,
    };

    struct PhysicsJoint {
        EntityRef target;
        PhysicsJointType type = PhysicsJointType::Fixed;
        glm::vec2 limit = glm::vec2();
        Transform localOffset = Transform();
        Transform remoteOffset = Transform();

        bool operator==(const PhysicsJoint &) const = default;
    };

    static const StructMetadata MetadataPhysicsJoint(typeid(PhysicsJoint),
        StructField::New("target", &PhysicsJoint::target),
        StructField::New("type", &PhysicsJoint::type),
        StructField::New("limit", &PhysicsJoint::limit),
        StructField::New("local_offset", &PhysicsJoint::localOffset),
        StructField::New("remote_offset", &PhysicsJoint::remoteOffset));

    struct PhysicsJoints {
        std::vector<PhysicsJoint> joints;

        void Add(const PhysicsJoint &joint) {
            joints.push_back(joint);
        }
    };

    static const StructMetadata MetadataPhysicsJoints(typeid(PhysicsJoints),
        StructField::New(&PhysicsJoints::joints, ~FieldAction::AutoApply));
    static Component<PhysicsJoints> ComponentPhysicsJoints("physics_joints", MetadataPhysicsJoints);

    template<>
    void Component<PhysicsJoints>::Apply(PhysicsJoints &dst, const PhysicsJoints &src, bool liveTarget);
} // namespace ecs
