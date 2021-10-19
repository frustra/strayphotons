#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"

#include <string>

namespace sp {
    class ConsoleBindingManager {
    public:
        ConsoleBindingManager();

        void SetConsoleInputCommand(ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::Script, ecs::EventInput>> lock,
            std::string eventName,
            std::string command);

    private:
        // CFunc
        void BindKey(string keyName, string command);

        CFuncCollection funcs;

        ecs::NamedEntity consoleInputEntity;
        ecs::NamedEntity keyboardEntity;
    };
} // namespace sp
