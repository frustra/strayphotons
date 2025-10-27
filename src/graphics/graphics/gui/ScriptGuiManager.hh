/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"
#include "ecs/ScriptDefinition.hh"
#include "graphics/gui/WorldGuiManager.hh"

namespace sp {
    class ScriptGuiManager : public WorldGuiManager {
    public:
        ScriptGuiManager(ecs::Entity gui, const std::string &name, std::shared_ptr<ecs::ScriptState> scriptState);

        bool SetGuiContext() override;
        void BeforeFrame() override;
        void DefineWindows() override;
        ImDrawData *GetDrawData(glm::vec2 resolution, glm::vec2 scale, float deltaTime) const override;

    private:
        std::shared_ptr<ecs::ScriptState> scriptState;
    };
} // namespace sp
