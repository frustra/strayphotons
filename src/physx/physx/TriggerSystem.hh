#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class TriggerSystem {
    public:
        TriggerSystem() {}

        void Frame();
    };
} // namespace sp
