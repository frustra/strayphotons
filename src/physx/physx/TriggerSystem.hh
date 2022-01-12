#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class TriggerSystem {
    public:
        TriggerSystem();
        ~TriggerSystem();

        void Frame();

        ecs::ComponentObserver<ecs::TriggerGroup> triggerGroupObserver;
    };
} // namespace sp
