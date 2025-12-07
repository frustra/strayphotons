/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ScriptGuiDefinition.hh"

#include "graphics/GenericCompositor.hh"

#include <imgui_internal.h>

namespace ecs {
    ScriptGuiDefinition::ScriptGuiDefinition(std::shared_ptr<ScriptState> state, EntityRef guiDefinitionEntity)
        : GuiDefinition(state->definition.name,
              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoBackground),
          weakState(state), guiDefinitionEntity(guiDefinitionEntity) {}

    ScriptGuiDefinition::~ScriptGuiDefinition() {
        context = {};
    }

    bool ScriptGuiDefinition::BeforeFrame(Entity ent) {
        ZoneScoped;
        context = {};
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

            {
                auto lock = ecs::StartTransaction<ecs::SendEventsLock>();

                ecs::Entity gui = guiDefinitionEntity.Get(lock);
                for (ImGuiInputEvent &event : ImGui::GetCurrentContext()->InputEventsQueue) {
                    ecs::EventBytes data = {};
                    static_assert(sizeof(ImGuiInputEvent) <= sizeof(data));
                    std::copy_n(reinterpret_cast<const char *>(&event), sizeof(ImGuiInputEvent), data.begin());
                    ecs::Event inputEvent("/gui/imgui_input", gui, data);
                    ecs::EventBindings::SendEvent(lock, guiDefinitionEntity, inputEvent);
                }
            }

            auto &[beforeFrame, renderGui] = std::get<ecs::GuiRenderFuncs>(state.definition.callback);
            if (beforeFrame && renderGui && beforeFrame(state, ent)) {
                auto &io = ImGui::GetIO();
                ImGuiViewport *imguiViewport = ImGui::GetMainViewport();
                // TODO: Viewport is one frame behind here, and undefined on first frame.
                Assertf(imguiViewport != nullptr, "ImGui::GetMainViewport() returned null");
                glm::vec2 displaySize = {imguiViewport->WorkSize.x, imguiViewport->WorkSize.y};
                glm::vec2 scale = {io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y};
                renderGui(state, ent, displaySize, scale, io.DeltaTime, context.drawData);
                return !context.drawData.drawCommands.empty();
            }
            return false;
        });
    }

    void ScriptGuiDefinition::PreDefine(Entity ent) {
        ImGuiViewport *imguiViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize(imguiViewport->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    }

    void ScriptGuiDefinition::DefineContents(Entity ent) {
        ImGui::Dummy(ImGui::GetContentRegionAvail());

        auto &io = ImGui::GetIO();
        sp::GenericCompositor *compositor = (sp::GenericCompositor *)io.UserData;
        if (!context.drawData.drawCommands.empty() && compositor) {
            context.compositor = compositor;
            auto minPos = ImGui::GetItemRectMin();
            auto maxPos = ImGui::GetItemRectMax();
            context.viewport = glm::vec4(minPos.x, minPos.y, maxPos.x - minPos.x, maxPos.y - minPos.y);

            context.scale = glm::vec2(io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            context.viewport.x *= context.scale.x;
            context.viewport.y *= context.scale.y;
            context.viewport.z *= context.scale.x;
            context.viewport.w *= context.scale.y;

            ImDrawList *drawList = ImGui::GetForegroundDrawList();
            drawList->AddCallback(
                [](const ImDrawList *drawList, const ImDrawCmd *cmd) {
                    if (cmd && cmd->UserCallbackData) {
                        CallbackContext *context = static_cast<CallbackContext *>(cmd->UserCallbackData);
                        if (!context->compositor) return;
                        context->compositor->DrawGui(context->drawData, (glm::ivec4)context->viewport, context->scale);
                    }
                },
                &context);
        }
    }

    void ScriptGuiDefinition::PostDefine(Entity ent) {
        ImGui::PopStyleVar(2);
    }
} // namespace ecs
