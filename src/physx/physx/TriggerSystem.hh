#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class TriggerSystem {
    public:
        TriggerSystem();
        ~TriggerSystem();

        void Frame(ecs::Lock<ecs::Read<ecs::Name, ecs::TriggerGroup, ecs::TransformSnapshot>,
            ecs::Write<ecs::TriggerArea, ecs::SignalOutput>,
            ecs::SendEventsLock> lock);

        ecs::ComponentObserver<ecs::TriggerGroup> triggerGroupObserver;
    };
} // namespace sp
