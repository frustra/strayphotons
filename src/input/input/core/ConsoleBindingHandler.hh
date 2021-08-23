#pragma once

#include "core/CFunc.hh"
#include "ecs/NamedEntity.hh"
#include "input/core/KeyCodes.hh"

#include <robin_hood.h>

namespace sp {
    class ConsoleBindingHandler {
    public:
        ConsoleBindingHandler();

        void Frame();

    private:
        // CFunc
        void BindKey(string args);

        CFuncCollection funcs;

        Tecs::Entity bindingEntity;
        ecs::NamedEntity keyboardEntity;
    };
} // namespace sp
