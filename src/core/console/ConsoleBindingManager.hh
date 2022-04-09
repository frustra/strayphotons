#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <string>

namespace sp {
    class ConsoleBindingManager {
    public:
        ConsoleBindingManager();

    private:
        // CFunc
        void BindKey(std::string keyName, std::string command);

        CFuncCollection funcs;

        ecs::EntityRef consoleInputEntity, keyboardEntity;
    };
} // namespace sp
