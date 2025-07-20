/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Events.hh"

namespace sp {

    class EditorSystem {
    public:
        EditorSystem();

        void OpenEditor(std::string targetName, bool flatMode = true);
        void ToggleTray();

    private:
        ecs::EntityRef pickerEntity = ecs::Name("editor", "entity_picker");
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");
        ecs::Entity previousTarget;

        CFuncCollection funcs;
    };

} // namespace sp
