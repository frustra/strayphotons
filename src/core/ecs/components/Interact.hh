#pragma once

#include <ecs/Components.hh>

namespace physx {
    class PxRigidDynamic;
}

namespace ecs {
    struct InteractController {
        Tecs::Entity target;
    };

    static Component<InteractController> ComponentInteractController("interact_controller");

    template<>
    bool Component<InteractController>::Load(sp::Scene *scene, InteractController &dst, const picojson::value &src);
} // namespace ecs
