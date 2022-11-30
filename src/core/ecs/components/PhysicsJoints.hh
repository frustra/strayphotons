#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

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
        glm::vec3 localOffset = glm::vec3(), remoteOffset = glm::vec3();
        glm::quat localOrient = glm::quat(), remoteOrient = glm::quat();

        bool operator==(const PhysicsJoint &) const = default;
    };

    struct PhysicsJoints {
        vector<PhysicsJoint> joints;

        void Add(const PhysicsJoint &joint) {
            joints.push_back(joint);
        }
    };

    static StructMetadata MetadataPhysicsJoints(typeid(PhysicsJoints));
    static Component<PhysicsJoints> ComponentPhysicsJoints("physics_joints", MetadataPhysicsJoints);

    template<>
    bool Component<PhysicsJoints>::Load(const EntityScope &scope, PhysicsJoints &dst, const picojson::value &src);
    template<>
    void Component<PhysicsJoints>::Apply(const PhysicsJoints &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
