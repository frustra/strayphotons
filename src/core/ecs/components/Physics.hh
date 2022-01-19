#pragma once

#include "core/EnumArray.hh"
#include "ecs/Components.hh"

#include <Tecs.hh>
#include <glm/glm.hpp>
#include <memory>

namespace sp {
    class Model;
}

namespace physx {
    class PxRigidActor;
    class PxScene;
    class PxControllerManager;
} // namespace physx

namespace ecs {
    struct Physics {
        Physics() {}
        Physics(std::shared_ptr<const sp::Model> model, bool dynamic = true, float density = 1.0f)
            : model(model), dynamic(dynamic), density(density) {}

        std::shared_ptr<const sp::Model> model;

        bool dynamic = true;
        bool kinematic = false; // only dynamic actors can be kinematic
        bool decomposeHull = false;
        float density = 1.0f;

        Tecs::Entity constraint;
        float constraintMaxDistance = 0.0f;
        glm::vec3 constraintOffset;
        glm::quat constraintRotation;

        // For use by PhysxManager only
        physx::PxRigidActor *actor = nullptr;
        glm::vec3 scale = glm::vec3(1.0); // Current scale of physics model according to PhysX

        void SetConstraint(Tecs::Entity target,
            float maxDistance = 0.0f,
            glm::vec3 offset = glm::vec3(),
            glm::quat rotation = glm::quat()) {
            constraint = target;
            constraintMaxDistance = maxDistance;
            constraintOffset = offset;
            constraintRotation = rotation;
        }

        void RemoveConstraint() {
            SetConstraint(Tecs::Entity());
        }
    };

    struct PhysicsQuery {
        float maxRaycastDistance = 0.0f;
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
