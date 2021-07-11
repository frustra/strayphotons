#pragma once

#include "ecs/Components.hh"

#include <glm/glm.hpp>
#include <memory>

namespace sp {
    class Model;
}

namespace physx {
    class PxRigidActor;
}

namespace ecs {
    struct PhysxActorDesc {
        bool dynamic = true;
        bool kinematic = false; // only dynamic actors can be kinematic
        bool decomposeHull = false;

        // Initial values
        glm::mat4 transform = glm::mat4(1);
        glm::vec3 scale = glm::vec3(1);
        float density = 1.0f;
    };

    struct Physics {
        Physics() {}
        Physics(std::shared_ptr<sp::Model> model, PhysxActorDesc desc) : model(model), desc(desc) {}
        Physics(physx::PxRigidActor *actor, std::shared_ptr<sp::Model> model, PhysxActorDesc desc)
            : actor(actor), model(model), desc(desc) {}

        physx::PxRigidActor *actor = nullptr;
        std::shared_ptr<sp::Model> model;
        PhysxActorDesc desc;

        glm::vec3 scale = glm::vec3(1.0);
    };

    static Component<Physics> ComponentPhysics("physics");

    template<>
    bool Component<Physics>::Load(Lock<Read<ecs::Name>> lock, Physics &dst, const picojson::value &src);
} // namespace ecs
