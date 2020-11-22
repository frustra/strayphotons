#pragma once

#include "Common.hh"
#include "physx/PhysxManager.hh"

#include <PxRigidDynamic.h>
#include <ecs/Components.hh>

namespace ecs {
    struct InteractController {
        void PickUpObject(ecs::Entity entity);

        physx::PxRigidDynamic *target = nullptr;
        sp::PhysxManager *manager;
    };

    static Component<InteractController> ComponentInteractController("interact_controller");
} // namespace ecs
