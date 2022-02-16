#pragma once

#include "core/RegisteredThread.hh"

namespace sp {

    class GameLogic : public RegisteredThread {
    public:
        GameLogic(bool stepMode);

        void StartThread();

        void PrintDebug();
        void PrintEvents(std::string entityName);
        void PrintSignals(std::string entityName);

    private:
        void Frame() override;

        bool stepMode;
        CFuncCollection funcs;
    };

} // namespace sp
