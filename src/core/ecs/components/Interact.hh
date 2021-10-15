#pragma once

#include <ecs/Components.hh>

namespace sp {
    class PhysxManager;
}

namespace physx {
    class PxRigidDynamic;
}

namespace ecs {
    struct InteractController {
        physx::PxRigidDynamic *target = nullptr;
    };

    static Component<InteractController> ComponentInteractController("interact_controller");
} // namespace ecs
