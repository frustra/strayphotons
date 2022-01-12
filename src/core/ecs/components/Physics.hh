#pragma once

#include "ecs/Components.hh"

#include <Tecs.hh>
#include <glm/glm.hpp>
#include <memory>
#include <type_traits>

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

        void SetConstraint(Tecs::Entity target, float maxDistance = 0.0f, glm::vec3 offset = glm::vec3(), glm::quat rotation = glm::quat()) {
            constraint = target;
            constraintMaxDistance = maxDistance;
            constraintOffset = offset;
            constraintRotation = rotation;
        }

        void RemoveConstraint() {
            constraint = Tecs::Entity();
            constraintMaxDistance = 0.0f;
            constraintOffset = glm::vec3();
            constraintRotation = glm::quat();
        }
    };

    static Component<Physics> ComponentPhysics("physics");

    template<>
    bool Component<Physics>::Load(sp::Scene *scene, Physics &dst, const picojson::value &src);
} // namespace ecs
