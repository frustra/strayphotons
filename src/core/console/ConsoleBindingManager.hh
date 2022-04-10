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

        ecs::EntityRef consoleInputEntity = ecs::Name("console", "input");
        ecs::EntityRef keyboardEntity = ecs::Name("input", "keyboard");
    };
} // namespace sp
