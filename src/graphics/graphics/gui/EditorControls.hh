#pragma once

#include "ecs/EcsImpl.hh"
#include "editor/EditorSystem.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace sp {
    template<typename T>
    bool AddImGuiElement(const char *name, T &value) {
        Logf("Missing ImGuiElement conversion: %s", typeid(T).name());
        // } else if (field.type == ecs::FieldType::Quat) {
        //     auto value = *field.Access<glm::quat>(component);
        // } else if (field.type == ecs::FieldType::EntityRef) {
        //     auto value = *field.Access<ecs::EntityRef>(component);
        // } else if (field.type == ecs::FieldType::Transform) {
        //     auto value = *field.Access<ecs::Transform>(component);
        // } else if (field.type == ecs::FieldType::AnimationStates) {
        //     auto value = *field.Access<std::vector<ecs::AnimationState>>(component);
        // } else if (field.type == ecs::FieldType::InterpolationMode) {
        //     auto value = *field.Access<ecs::InterpolationMode>(component);
        // } else if (field.type == ecs::FieldType::VisibilityMask) {
        //     auto value = *field.Access<ecs::VisibilityMask>(component);
        return false;
    }

    template<>
    bool AddImGuiElement(const char *name, bool &value) {
        return ImGui::Checkbox(name, &value);
    }
    template<>
    bool AddImGuiElement(const char *name, int32_t &value) {
        return ImGui::DragScalar(name, ImGuiDataType_S32, &value, 1.0f, NULL, NULL, "%d");
    }
    template<>
    bool AddImGuiElement(const char *name, uint32_t &value) {
        return ImGui::DragScalar(name, ImGuiDataType_U32, &value, 1.0f, NULL, NULL, "%u");
    }
    template<>
    bool AddImGuiElement(const char *name, size_t &value) {
        return ImGui::DragScalar(name, ImGuiDataType_U64, &value, 1.0f, NULL, NULL, "%u");
    }
    template<>
    bool AddImGuiElement(const char *name, sp::angle_t &value) {
        return ImGui::SliderAngle(name, &value.radians(), 0.0f, 360.0f);
    }
    template<>
    bool AddImGuiElement(const char *name, float &value) {
        return ImGui::DragFloat(name, &value, 0.01f);
    }
    template<>
    bool AddImGuiElement(const char *name, glm::vec2 &value) {
        return ImGui::DragFloat2(name, (float *)&value, 0.01f);
    }
    template<>
    bool AddImGuiElement(const char *name, glm::vec3 &value) {
        return ImGui::DragFloat3(name, (float *)&value, 0.01f);
    }
    template<>
    bool AddImGuiElement(const char *name, glm::vec4 &value) {
        return ImGui::DragFloat4(name, (float *)&value, 0.01f);
    }
    template<>
    bool AddImGuiElement(const char *name, glm::ivec2 &value) {
        return ImGui::DragInt2(name, (int *)&value);
    }
    template<>
    bool AddImGuiElement(const char *name, glm::ivec3 &value) {
        return ImGui::DragInt3(name, (int *)&value);
    }
    template<>
    bool AddImGuiElement(const char *name, std::string &value) {
        return ImGui::InputText(name, &value);
    }
    template<>
    bool AddImGuiElement(const char *name, ecs::EntityRef &value) {
        ImGui::Text("%s: %s / %s", name, value.Name().String().c_str(), std::to_string(value.GetLive()).c_str());
        return false;
    }
    template<>
    bool AddImGuiElement(const char *name, ecs::Transform &value) {
        // TODO: Add grab handle in view
        auto text = std::string(name) + ".position";
        bool changed = ImGui::DragFloat3(text.c_str(), (float *)&value.matrix[3], 0.01f);

        text = std::string(name) + ".rotation";
        glm::vec3 angles = glm::degrees(glm::eulerAngles(value.GetRotation()));
        if (ImGui::SliderFloat3(text.c_str(), (float *)&angles, -360.0f, 360.0f, "%.1f deg")) {
            value.SetRotation(glm::quat(glm::radians(angles)));
            changed = true;
        }

        text = std::string(name) + ".scale";
        glm::vec3 scale = value.GetScale();
        if (ImGui::DragFloat3(text.c_str(), (float *)&scale, 0.01f)) {
            value.SetScale(scale);
            changed = true;
        }
        return changed;
    }

    template<typename T>
    bool AddFieldControls(const ecs::ComponentField &field,
        const ecs::ComponentBase &comp,
        ecs::Entity target,
        const void *component) {
        auto value = *field.Access<T>(component);
        if (!AddImGuiElement(field.name ? field.name : comp.name, value)) return true;

        GEditor.PushEdit([target, value, &comp, &field]() {
            auto lock = ecs::World.StartTransaction<ecs::WriteAll>();
            void *component = comp.Access(lock, target);
            *field.Access<T>(component) = value;
        });
        return true;
    }

} // namespace sp
