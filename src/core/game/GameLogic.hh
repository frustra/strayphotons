#pragma once

#include "core/RegisteredThread.hh"

namespace sp {

    class GameLogic : public RegisteredThread {
    public:
        GameLogic(bool stepMode);

        void StartThread();

        void PrintDebug();
        void JsonDump(std::string entityName);
        void PrintEvents();
        void PrintSignals();
        void PrintSignal(std::string signalName);

    private:
        void Frame() override;

        bool stepMode;
        CFuncCollection funcs;
    };

} // namespace sp
