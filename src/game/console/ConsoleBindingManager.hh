/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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
