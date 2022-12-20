#include "InspectorGui.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/StructFieldTypes.hh"
#include "editor/EditorSystem.hh"
#include "graphics/gui/EditorControls.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <picojson/picojson.h>

namespace sp {
    InspectorGui::InspectorGui(const string &name)
        : GuiWindow(name, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove) {
        context = make_shared<EditorContext>();
        std::thread([ctx = context]() {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::EventInput>>();

            auto inspector = ctx->inspectorEntity.Get(lock);
            if (!inspector.Has<ecs::EventInput>(lock)) return;

            auto &eventInput = inspector.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, ctx->events, EDITOR_EVENT_EDIT_TARGET);
        }).detach();
    }

    void InspectorGui::PreDefine() {
        auto *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize(ImVec2(500, viewport->Size.y));
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y),
            ImGuiCond_None,
            ImVec2(1, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.01f, 0.01f, 0.01f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.15f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.10f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.10f, 0.10f, 0.35f, 1.0f));
    }

    void InspectorGui::PostDefine() {
        ImGui::PopStyleColor(5);
    }

    void InspectorGui::DefineContents() {
        if (!context) return;
        ZoneScoped;
        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput>>();

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, context->events, event)) {
                if (event.name != EDITOR_EVENT_EDIT_TARGET) continue;

                auto newTarget = std::get_if<ecs::Entity>(&event.data);
                if (!newTarget) {
                    Errorf("Invalid editor event: %s", event.toString());
                } else {
                    context->targetEntity = *newTarget;
                }
            }
        }
        if (context->targetEntity) {
            if (ImGui::Button("Entity Selector")) {
                context->targetEntity = {};
                context->RefreshEntityTree();
            } else {
                auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll>();
                auto liveLock = ecs::StartTransaction<ecs::ReadAll>();

                context->ShowEntityEditControls(liveLock, stagingLock);
            }
        }
        if (!context->targetEntity) {
            context->ShowEntityTree();
        }
    }
} // namespace sp
