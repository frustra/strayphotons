#pragma once

#include "console/CFunc.hh"
#include "core/RegisteredThread.hh"
#include "ecs/EntityRef.hh"

namespace sp {

    class GameLogic : public RegisteredThread {
    public:
        GameLogic(bool stepMode);

        void StartThread();

    private:
        void Frame() override;

        bool stepMode;
        CFuncCollection funcs;
    };

} // namespace sp
