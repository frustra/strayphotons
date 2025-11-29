/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ScriptGuiDefinition.hh"

#include "graphics/GenericCompositor.hh"

namespace ecs {
    ScriptGuiDefinition::ScriptGuiDefinition(std::shared_ptr<ScriptState> state)
        : GuiDefinition(state->definition.name), weakState(state) {}

    bool ScriptGuiDefinition::BeforeFrame(Entity ent) {
        ZoneScoped;
        drawData = nullptr;
        return ecs::GetScriptManager().WithGuiScriptLock([&] {
            auto activeState = weakState.lock();
            if (!activeState) return false;
            ecs::ScriptState &state = *activeState;

            Assertf(state.definition.type == ScriptType::GuiScript,
                "ScriptGuiDefinition %s is wrong type: %s",
                state.definition.name,
                state.definition.type);
            Assertf(std::holds_alternative<GuiRenderFuncs>(state.definition.callback),
                "ScriptGuiDefinition %s has invalid callback type",
                state.definition.name);

            auto &[beforeFrame, renderGui] = std::get<ecs::GuiRenderFuncs>(state.definition.callback);
            if (beforeFrame && renderGui && beforeFrame(state, ent)) {
                auto &io = ImGui::GetIO();
                ImGuiViewport *imguiViewport = ImGui::GetMainViewport();
                Assertf(imguiViewport != nullptr, "ImGui::GetMainViewport() returned null");
                glm::vec2 displaySize = {imguiViewport->WorkSize.x, imguiViewport->WorkSize.y};
                glm::vec2 scale = {io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y};
                drawData = renderGui(state, ent, displaySize, scale, io.DeltaTime);
                return drawData != nullptr;
            }
            return false;
        });
    }

    void ScriptGuiDefinition::DefineContents(Entity ent) {
        ImGuiViewport *imguiViewport = ImGui::GetMainViewport();
        ImGui::Dummy(imguiViewport->WorkSize);

        auto &io = ImGui::GetIO();
        sp::GenericCompositor *compositor = (sp::GenericCompositor *)io.UserData;
        if (drawData && compositor) {
            drawData->ScaleClipRects(io.DisplayFramebufferScale);

            // TODO: Make a copy of ImDrawData here
            struct CallbackContext {
                ImDrawData *drawData;
                sp::GenericCompositor *compositor;
                glm::ivec4 viewport;
                glm::vec2 scale;
            } context = {drawData,
                compositor,
                glm::ivec4(imguiViewport->WorkPos.x,
                    imguiViewport->WorkPos.y,
                    imguiViewport->WorkSize.x,
                    imguiViewport->WorkSize.y),
                glm::vec2(io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y)};

            ImDrawList *drawList = ImGui::GetForegroundDrawList();
            drawList->AddCallback(
                [](const ImDrawList *drawList, const ImDrawCmd *cmd) {
                    if (cmd && cmd->UserCallbackData && cmd->UserCallbackDataSize == sizeof(CallbackContext)) {
                        CallbackContext *context = static_cast<CallbackContext *>(cmd->UserCallbackData);
                        Assertf(context->drawData && context->compositor,
                            "Compositor::DrawGui ImGui callback called with invalid context");
                        context->compositor->DrawGui(context->drawData, context->viewport, context->scale);
                    }
                },
                &context,
                sizeof(context));
        }
    }
} // namespace ecs
