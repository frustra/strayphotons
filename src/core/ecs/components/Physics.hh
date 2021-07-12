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
} // namespace physx

namespace ecs {
    struct Physics {
        Physics() {}
        Physics(std::shared_ptr<sp::Model> model, bool dynamic = true, float density = 1.0f)
            : model(model), dynamic(dynamic), density(density) {}

        std::shared_ptr<sp::Model> model;

        bool dynamic = true;
        bool kinematic = false; // only dynamic actors can be kinematic
        bool decomposeHull = false;
        float density = 1.0f;

        // For use by PhysxManager only
        physx::PxRigidActor *actor = nullptr;
        glm::vec3 scale = glm::vec3(1.0); // Current scale of physics model according to PhysX
    };

    struct PhysicsScene {
        std::shared_ptr<physx::PxScene> scene;

        PhysicsScene() {}
        PhysicsScene(std::shared_ptr<physx::PxScene> scene) : scene(scene) {}
    };

    static Component<Physics> ComponentPhysics("physics");

    template<>
    bool Component<Physics>::Load(Lock<Read<ecs::Name>> lock, Physics &dst, const picojson::value &src);
} // namespace ecs

template<>
struct Tecs::is_global_component<ecs::PhysicsScene> : std::true_type {};
