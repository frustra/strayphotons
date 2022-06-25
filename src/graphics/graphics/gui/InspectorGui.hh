#pragma once

#include "graphics/gui/GuiContext.hh"

#include <imgui/imgui.h>

namespace sp {
    class InspectorGui : public GuiWindow {
    public:
        InspectorGui(const string &name) : GuiWindow(name) {}
        virtual ~InspectorGui() {}

        void DefineContents() {
            children.clear();

            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::TransformTree>>();
            auto entities = lock.EntitiesWith<ecs::TransformTree>();

            children.resize(entities.back().index + 1);

            for (auto &ent : entities) {
                auto &tree = ent.Get<ecs::TransformTree>(lock);
                if (tree.parent) children[tree.parent.Get(lock).index].push_back(ent);
            }

            for (auto &ent : entities) {
                auto &tree = ent.Get<ecs::TransformTree>(lock);
                if (tree.parent) continue;
                AppendEntity(0, ent, lock);
            }
        }

    private:
        void AppendEntity(int depth, ecs::Entity ent, ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree>> lock) {
            auto name = ecs::ToString(lock, ent);

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
            if (children[ent.index].empty()) flags |= ImGuiTreeNodeFlags_Leaf;

            bool open = ImGui::TreeNodeEx((void *)(uintptr_t)ent.index, flags, "%s", name.c_str());
            if (!open) return;

            for (auto child : children[ent.index]) {
                AppendEntity(depth + 1, child, lock);
            }

            ImGui::TreePop();
        }

    private:
        vector<vector<ecs::Entity>> children;
    };
} // namespace sp
