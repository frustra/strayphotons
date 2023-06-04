#pragma once

#include "console/CFunc.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Events.hh"

namespace ecs {
    class EventQueue;
}

namespace sp {

    class GameLogic : public RegisteredThread {
    public:
        GameLogic(ecs::EventQueue &windowInputQueue, bool stepMode);

        void StartThread();

        static void UpdateInputEvents(const ecs::Lock<ecs::SendEventsLock, ecs::Write<ecs::Signals>> &lock,
            ecs::EventQueue &inputQueue);

    private:
        void Frame() override;

        ecs::EventQueue &windowInputQueue;

        bool stepMode;
        CFuncCollection funcs;
    };

} // namespace sp
