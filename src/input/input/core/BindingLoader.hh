#pragma once

#include "core/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"

#include <string>

namespace sp {
    static const char *const InputBindingConfigPath = "input_bindings.json";

    class BindingLoader {
    public:
        BindingLoader();

        void Load(std::string bindingConfigPath);
        void SetConsoleInputCommand(ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::Script, ecs::EventInput>> lock,
                                    std::string eventName,
                                    std::string command);

    private:
        // CFunc
        void BindKey(string args);

        CFuncCollection funcs;

        ecs::NamedEntity consoleInputEntity;
        ecs::NamedEntity keyboardEntity;
    };
} // namespace sp
