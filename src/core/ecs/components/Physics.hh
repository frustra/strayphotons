#pragma once

#include "ecs/Components.hh"

#include <Tecs.hh>
#include <glm/glm.hpp>
#include <memory>

namespace sp {
    class Model;
}

namespace physx {
    class PxRigidActor;
    class PxFixedJoint;
    class PxRevoluteJoint;
    class PxScene;
    class PxControllerManager;
} // namespace physx

namespace ecs {
    enum class PhysicsGroup : uint16_t {
        NoClip = 0,
        World,
        Player,
        Count,
    };

    enum PhysicsGroupMask {
        PHYSICS_GROUP_NOCLIP = 1 << (size_t)PhysicsGroup::NoClip,
        PHYSICS_GROUP_WORLD = 1 << (size_t)PhysicsGroup::World,
        PHYSICS_GROUP_PLAYER = 1 << (size_t)PhysicsGroup::Player,
    };

    enum class PhysicsConstraintType {
        Fixed = 0,
        Distance,
        Spherical,
        Hinge,
        Slider,
        ForceLimit,
        Count,
    };

    struct Physics {
        Physics() {}
        Physics(std::shared_ptr<const sp::Model> model,
            PhysicsGroup group = PhysicsGroup::World,
            bool dynamic = true,
            float density = 1.0f)
            : model(model), group(group), dynamic(dynamic), density(density) {}

        std::shared_ptr<const sp::Model> model;

        PhysicsGroup group = PhysicsGroup::World;
        bool dynamic = true;
        bool kinematic = false; // only dynamic actors can be kinematic
        bool decomposeHull = false;
        float density = 1.0f;

        Tecs::Entity constraint;
        PhysicsConstraintType constraintType;
        float constraintMaxDistance = 0.0f;
        glm::vec3 constraintOffset;
        glm::quat constraintRotation;

        // Output fields of PhysxManager
        physx::PxRigidActor *actor = nullptr;
        physx::PxFixedJoint *fixedJoint = nullptr;
        physx::PxRevoluteJoint *hingeJoint = nullptr;
        glm::vec3 scale = glm::vec3(1.0); // Current scale of physics model according to PhysX
        glm::vec3 centerOfMass = glm::vec3(0.0); // The calculated center of mass of the object (relative to Transform)

        void SetConstraint(Tecs::Entity target,
            PhysicsConstraintType type,
            float maxDistance = 0.0f,
            glm::vec3 offset = glm::vec3(),
            glm::quat rotation = glm::quat()) {
            constraint = target;
            constraintType = type;
            constraintMaxDistance = maxDistance;
            constraintOffset = offset;
            constraintRotation = rotation;
        }

        void RemoveConstraint() {
            SetConstraint(Tecs::Entity(), PhysicsConstraintType::Count);
        }
    };

    struct PhysicsQuery {
        float raycastQueryDistance = 0.0f;
        PhysicsGroupMask raycastQueryFilterGroup = PHYSICS_GROUP_WORLD;

        Tecs::Entity raycastHitTarget;
        glm::vec3 raycastHitPosition;
        float raycastHitDistance = 0.0f;
    };

    static Component<Physics> ComponentPhysics("physics");
    static Component<PhysicsQuery> ComponentPhysicsQuery("physics_query");

    template<>
    bool Component<Physics>::Load(sp::Scene *scene, Physics &dst, const picojson::value &src);
    template<>
    bool Component<PhysicsQuery>::Load(sp::Scene *scene, PhysicsQuery &dst, const picojson::value &src);
} // namespace ecs
