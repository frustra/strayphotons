#pragma once

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <magic_enum.hpp>

namespace sp {
    template<typename T>
    bool AddImGuiElement(const std::string &name, T &value) {
        bool changed = false;
        if constexpr (std::is_enum_v<T>) {
            auto items = magic_enum::enum_entries<T>();
            if constexpr (magic_enum::detail::is_flags_v<T>) {
                if (ImGui::BeginListBox(name.c_str(), ImVec2(0, 5.25 * ImGui::GetTextLineHeightWithSpacing()))) {
                    for (auto &item : items) {
                        if (item.second.empty()) continue;
                        const bool is_selected = (value & item.first) == item.first;
                        if (ImGui::Selectable(item.second.data(), is_selected)) {
                            value ^= item.first;
                            changed = true;
                        }
                    }
                    ImGui::EndListBox();
                }
            } else {
                std::string enumName(magic_enum::enum_name(value));
                if (ImGui::BeginCombo(name.c_str(), enumName.c_str())) {
                    for (auto &item : items) {
                        if (item.second.empty()) continue;
                        const bool is_selected = item.first == value;
                        if (ImGui::Selectable(item.second.data(), is_selected)) {
                            value = item.first;
                            changed = true;
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        } else {
            picojson::value jsonValue;
            json::Save({}, jsonValue, value);
            auto separator = name.find("##");
            Assertf(separator != std::string::npos, "ImGuiElement name invalid: %s", name);
            ImGui::Text("%s: %s", name.substr(0, separator).c_str(), jsonValue.serialize(true).c_str());
        }
        return changed;
    }

    template<>
    bool AddImGuiElement(const std::string &name, bool &value) {
        return ImGui::Checkbox(name.c_str(), &value);
    }
    template<>
    bool AddImGuiElement(const std::string &name, int32_t &value) {
        return ImGui::DragScalar(name.c_str(), ImGuiDataType_S32, &value, 1.0f, NULL, NULL, "%d");
    }
    template<>
    bool AddImGuiElement(const std::string &name, uint32_t &value) {
        return ImGui::DragScalar(name.c_str(), ImGuiDataType_U32, &value, 1.0f, NULL, NULL, "%u");
    }
    template<>
    bool AddImGuiElement(const std::string &name, size_t &value) {
        return ImGui::DragScalar(name.c_str(), ImGuiDataType_U64, &value, 1.0f, NULL, NULL, "%u");
    }
    template<>
    bool AddImGuiElement(const std::string &name, sp::angle_t &value) {
        return ImGui::SliderAngle(name.c_str(), &value.radians(), 0.0f, 360.0f);
    }
    template<>
    bool AddImGuiElement(const std::string &name, float &value) {
        return ImGui::DragFloat(name.c_str(), &value, 0.01f);
    }
    template<>
    bool AddImGuiElement(const std::string &name, glm::vec2 &value) {
        return ImGui::DragFloat2(name.c_str(), (float *)&value, 0.01f);
    }
    template<>
    bool AddImGuiElement(const std::string &name, glm::vec3 &value) {
        return ImGui::DragFloat3(name.c_str(), (float *)&value, 0.01f);
    }
    template<>
    bool AddImGuiElement(const std::string &name, glm::vec4 &value) {
        return ImGui::DragFloat4(name.c_str(), (float *)&value, 0.01f);
    }
    template<>
    bool AddImGuiElement(const std::string &name, color_t &value) {
        return ImGui::ColorEdit3(name.c_str(), (float *)&value);
    }
    template<>
    bool AddImGuiElement(const std::string &name, color_alpha_t &value) {
        return ImGui::ColorEdit4(name.c_str(), (float *)&value);
    }
    template<>
    bool AddImGuiElement(const std::string &name, glm::ivec2 &value) {
        return ImGui::DragInt2(name.c_str(), (int *)&value);
    }
    template<>
    bool AddImGuiElement(const std::string &name, glm::ivec3 &value) {
        return ImGui::DragInt3(name.c_str(), (int *)&value);
    }
    template<>
    bool AddImGuiElement(const std::string &name, glm::quat &value) {
        // TODO: Add grab handle for orientation
        glm::vec3 angles = glm::degrees(glm::eulerAngles(value));
        for (glm::length_t i = 0; i < angles.length(); i++) {
            if (angles[i] < 0.0f) angles[i] += 360.0f;
        }

        if (ImGui::SliderFloat3(name.c_str(), (float *)&angles, 0.0f, 360.0f, "%.1f deg")) {
            value = glm::quat(glm::radians(angles));
            return true;
        }
        return false;
    }
    template<>
    bool AddImGuiElement(const std::string &name, std::string &value) {
        return ImGui::InputText(name.c_str(), &value);
    }
    template<>
    bool AddImGuiElement(const std::string &name, ecs::EntityRef &value) {
        // TODO: Add entity selection / entry window
        auto separator = name.find("##");
        Assertf(separator != std::string::npos, "ImGuiElement name invalid: %s", name);
        ImGui::Text("%s: %s / %s",
            name.substr(0, separator).c_str(),
            value.Name().String().c_str(),
            std::to_string(value.GetLive()).c_str());
        return false;
    }
    template<>
    bool AddImGuiElement(const std::string &name, ecs::Transform &value) {
        // TODO: Add grab handle in view

        auto separator = name.find("##");
        Assertf(separator != std::string::npos, "ImGuiElement name invalid: %s", name);
        auto text = "position" + name.substr(separator);
        bool changed = ImGui::DragFloat3(text.c_str(), (float *)&value.matrix[3], 0.01f);

        text = "rotation" + name.substr(separator);
        glm::vec3 angles = glm::degrees(glm::eulerAngles(value.GetRotation()));
        for (glm::length_t i = 0; i < angles.length(); i++) {
            if (angles[i] < 0.0f) angles[i] += 360.0f;
            if (angles[i] >= 360.0f) angles[i] -= 360.0f;
        }
        if (ImGui::SliderFloat3(text.c_str(), (float *)&angles, 0.0f, 360.0f, "%.1f deg")) {
            value.SetRotation(glm::quat(glm::radians(angles)));
            changed = true;
        }

        text = "scale" + name.substr(separator);
        glm::vec3 scale = value.GetScale();
        if (ImGui::DragFloat3(text.c_str(), (float *)&scale, 0.01f)) {
            value.SetScale(scale);
            changed = true;
        }
        return changed;
    }
    template<>
    bool AddImGuiElement(const std::string &name, std::vector<ecs::AnimationState> &value) {
        for (auto &state : value) {
            picojson::value jsonValue;
            json::Save({}, jsonValue, state);
            ImGui::Text("%s", jsonValue.serialize(true).c_str());
        }
        return false;
    }
    template<>
    bool AddImGuiElement(const std::string &name, std::vector<ecs::ScriptState> &value) {
        for (auto &state : value) {
            picojson::value jsonValue;
            json::Save({}, jsonValue, state);
            ImGui::Text("%s", jsonValue.serialize(true).c_str());
        }
        return false;
    }

    template<typename T>
    void AddFieldControls(const ecs::StructField &field,
        const ecs::ComponentBase &comp,
        const std::shared_ptr<Scene> &scene,
        ecs::Entity target,
        const void *component) {
        auto value = *field.Access<T>(component);
        std::string elementName = field.name ? field.name : comp.name;
        elementName += "##";
        elementName += comp.name + std::to_string(field.fieldIndex);
        if (!AddImGuiElement(elementName, value)) return;

        if (ecs::IsLive(target)) {
            GetSceneManager().QueueAction(SceneAction::EditLiveECS,
                [target, value, &comp, &field](ecs::Lock<ecs::WriteAll> lock) {
                    void *component = comp.Access(lock, target);
                    *field.Access<T>(component) = value;
                });
        } else if (scene != nullptr) {
            GetSceneManager().QueueAction(SceneAction::EditStagingScene,
                scene->name,
                [target, value, &comp, &field](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                    void *component = comp.Access((ecs::Lock<ecs::WriteAll>)lock, target);
                    *field.Access<T>(component) = value;
                });
        } else {
            Errorf("Can't add ImGui field controls for null scene: %s", std::to_string(target));
        }
    }

} // namespace sp
