#pragma once

#include "core/RegisteredThread.hh"

namespace sp {

    class GameLogic : public RegisteredThread {
    public:
        GameLogic(bool exitOnEmptyQueue = false);

        void StartThread();

        void PrintEvents(std::string entityName);
        void PrintSignals(std::string entityName);

    private:
        void Frame() override;

        bool exitOnEmptyQueue;
        CFuncCollection funcs;
    };

} // namespace sp
