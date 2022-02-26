#pragma once

#include "console/CFunc.hh"
#include "core/LockFreeMutex.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"

#include <robin_hood.h>
#include <string>

namespace sp {
    class ConsoleBindingManager {
    public:
        ConsoleBindingManager();

    private:
        // CFunc
        void BindKey(string keyName, string command);

        CFuncCollection funcs;

        static inline ecs::NamedEntity consoleInputEntity = ecs::NamedEntity("console-input");
        static inline ecs::NamedEntity keyboardEntity = ecs::NamedEntity("keyboard");
    };
} // namespace sp
