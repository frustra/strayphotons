#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"
#include "core/LockFreeMutex.hh"

#include <string>
#include <robin_hood.h>

namespace sp {
    class ConsoleBindingManager {
    public:
        ConsoleBindingManager();

        static void SetConsoleInputCommand(
            ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::EventInput>> lock,std::string eventName, std::string command);

    private:
        // CFunc
        void BindKey(string keyName, string command);

        CFuncCollection funcs;

        LockFreeMutex bindingMutex;
        robin_hood::unordered_map<std::string, std::string> boundCommands;

        static inline ecs::NamedEntity consoleInputEntity = ecs::NamedEntity("console-input");
        static inline ecs::NamedEntity keyboardEntity = ecs::NamedEntity("keyboard");
    };
} // namespace sp
