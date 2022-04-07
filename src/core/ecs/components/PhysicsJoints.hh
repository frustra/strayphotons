#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace sp {
    class Gltf;
}

namespace physx {
    class PxJoint;
} // namespace physx

namespace ecs {
    enum class PhysicsJointType {
        Fixed = 0,
        Distance,
        Spherical,
        Hinge,
        Slider,
        Count,
    };

    struct PhysicsJoint {
        Entity target;
        PhysicsJointType type = PhysicsJointType::Count;
        glm::vec2 range = glm::vec2();
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

    static Component<PhysicsJoints> ComponentPhysicsJoints("physics_joints");

    template<>
    bool Component<PhysicsJoints>::Load(ScenePtr scenePtr,
        const Name &scope,
        PhysicsJoints &dst,
        const picojson::value &src);

    template<>
    void Component<PhysicsJoints>::ApplyComponent(Lock<ReadAll> srcLock,
        Entity src,
        Lock<AddRemove> dstLock,
        Entity dst);
} // namespace ecs
