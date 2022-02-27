#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"

#include <string>

namespace sp {
    class ConsoleBindingManager {
    public:
        ConsoleBindingManager();

        static void SetConsoleInputCommand(
            ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::Script, ecs::EventInput>> lock,
            std::string eventName,
            std::string command);

    private:
        // CFunc
        void BindKey(string keyName, string command);

        CFuncCollection funcs;

        static inline ecs::NamedEntity consoleInputEntity = ecs::NamedEntity("console", "input");
        static inline ecs::NamedEntity keyboardEntity = ecs::NamedEntity("input", "keyboard");
    };
} // namespace sp
