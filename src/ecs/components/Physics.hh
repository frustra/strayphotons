#pragma once

#include "physx/PhysxActorDesc.hh"

#include <PxActor.h>
#include <PxRigidDynamic.h>
#include <ecs/Components.hh>
#include <glm/glm.hpp>
#include <memory>

namespace sp {
    class Model;
}

namespace ecs {
    struct Physics {
        Physics() {}
        Physics(std::shared_ptr<sp::Model> model, sp::PhysxActorDesc desc) : model(model), desc(desc) {}
        Physics(physx::PxRigidActor *actor, std::shared_ptr<sp::Model> model, sp::PhysxActorDesc desc)
            : actor(actor), model(model), desc(desc) {}

        physx::PxRigidActor *actor = nullptr;
        std::shared_ptr<sp::Model> model;
        sp::PhysxActorDesc desc;

        glm::vec3 scale = glm::vec3(1.0);
    };

    static Component<Physics> ComponentPhysics("physics");

    template<>
    bool Component<Physics>::Load(Lock<Read<ecs::Name>> lock, Physics &dst, const picojson::value &src);
} // namespace ecs
