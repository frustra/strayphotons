/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ScriptGuiManager.hh"

#include "common/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptDefinition.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace sp {
    ScriptGuiManager::ScriptGuiManager(ecs::Entity gui,
        const std::string &name,
        std::shared_ptr<ecs::ScriptState> scriptState)
        : WorldGuiManager(gui, name), scriptState(scriptState) {}

    bool ScriptGuiManager::SetGuiContext() {
        ImGui::SetCurrentContext(imCtx);
        return false;
    }

    void ScriptGuiManager::BeforeFrame() {
        ZoneScoped;
        if (imCtx) {
            ImGui::SetCurrentContext(imCtx);
            WorldGuiManager::BeforeFrame();

            auto lock = ecs::StartTransaction<ecs::SendEventsLock>();

            ecs::Entity gui = guiEntity.Get(lock);

            for (ImGuiInputEvent &event : imCtx->InputEventsQueue) {
                ecs::EventBytes data = {};
                static_assert(sizeof(ImGuiInputEvent) <= sizeof(data));
                std::copy_n(reinterpret_cast<const char *>(&event), sizeof(ImGuiInputEvent), data.begin());
                ecs::Event inputEvent("/gui/imgui_input", gui, data);
                ecs::EventBindings::SendEvent(lock, guiEntity, inputEvent);
            }

            if (scriptState) {
                ecs::GetScriptManager().WithGuiScriptLock([&] {
                    ecs::ScriptState &state = *scriptState;
                    if (state.definition.type == ecs::ScriptType::GuiScript) {
                        Assertf(std::holds_alternative<ecs::GuiRenderFuncs>(state.definition.callback),
                            "Gui script %s has invalid callback type: GuiScript != GuiRender",
                            state.definition.name);
                        auto &[beforeFrame, renderGui] = std::get<ecs::GuiRenderFuncs>(state.definition.callback);
                        return beforeFrame(state, lock, gui);
                    }
                });
            }

            imCtx->InputEventsQueue.resize(0);
        }
    }

    void ScriptGuiManager::DefineWindows() {}

    GuiDrawData ScriptGuiManager::GetDrawData(glm::vec2 displaySize, glm::vec2 scale, float deltaTime) const {
        ecs::Entity gui = guiEntity.GetLive();
        if (gui && scriptState) {
            return ecs::GetScriptManager().WithGuiScriptLock([&] {
                ecs::ScriptState &state = *scriptState;
                if (state.definition.type == ecs::ScriptType::GuiScript) {
                    Assertf(std::holds_alternative<ecs::GuiRenderFuncs>(state.definition.callback),
                        "Gui script %s has invalid callback type: GuiScript != GuiRender",
                        state.definition.name);
                    auto &[beforeFrame, renderGui] = std::get<ecs::GuiRenderFuncs>(state.definition.callback);
                    return renderGui(state, gui, displaySize, scale, deltaTime);
                }
                return GuiDrawData{};
            });
        }
        return {};
    }

} // namespace sp
