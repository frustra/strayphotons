/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GuiContext.hh"

#include "ecs/EcsImpl.hh"
#include "graphics/gui/definitions/EntityPickerGui.hh"
#include "graphics/gui/definitions/InspectorGui.hh"
#include "graphics/gui/definitions/LobbyGui.hh"
#include "graphics/gui/definitions/SignalDisplayGui.hh"
#include "input/KeyCodes.hh"

#include <imgui/imgui.h>

namespace sp {
    static std::array fontList = {
        GuiFontDef{GuiFont::Primary, "DroidSans-Regular.ttf", 16.0f},
        GuiFontDef{GuiFont::Primary, "DroidSans-Regular.ttf", 32.0f},
        GuiFontDef{GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 25.0f},
        GuiFontDef{GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 32.0f},
    };

    std::span<GuiFontDef> GetGuiFontList() {
        return fontList;
    }

    void GuiContext::PushFont(GuiFont fontType, float fontSize) {
        auto &io = ImGui::GetIO();
        Assert(io.Fonts->Fonts.size() == fontList.size() + 1, "unexpected font list size");

        for (size_t i = 0; i < fontList.size(); i++) {
            auto &f = fontList[i];
            if (f.type == fontType && f.size == fontSize) {
                ImGui::PushFont(io.Fonts->Fonts[i + 1]);
                return;
            }
        }

        Abortf("missing font type %d with size %f", (int)fontType, fontSize);
    }

    GuiContext::GuiContext(const std::string &name) : name(name) {
        imCtx = ImGui::CreateContext();
        SetGuiContext();
    }

    GuiContext::~GuiContext() {
        SetGuiContext();
        ImGui::DestroyContext(imCtx);
        imCtx = nullptr;
    }

    bool GuiContext::SetGuiContext() {
        ImGui::SetCurrentContext(imCtx);
        return true;
    }

    void GuiContext::BeforeFrame() {
        SetGuiContext();
    }

    ImDrawData *GuiContext::GetDrawData(glm::vec2, glm::vec2, float) const {
        return ImGui::GetDrawData();
    }

    void GuiContext::Attach(const Ref &component) {
        if (!sp::contains(components, component)) components.emplace_back(component);
    }

    void GuiContext::Detach(const Ref &component) {
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end()) components.erase(it);
    }

    GuiContext::Ref LookupInternalGui(const std::string &windowName) {
        if (windowName == "lobby") {
            static const auto lobby = make_shared<LobbyGui>(windowName);
            return lobby;
        } else if (windowName == "entity_picker") {
            static const auto entityPicker = make_shared<EntityPickerGui>(windowName);
            return entityPicker;
        } else if (windowName == "inspector") {
            static const auto inspector = make_shared<InspectorGui>(windowName);
            return inspector;
            // } else if (windowName == "signal_display") {
            //     static const auto signalDisplay = make_shared<SignalDisplayGui>(windowName);
            //     return signalDisplay;
        }
        return std::weak_ptr<ecs::GuiRenderable>();
    }

    std::shared_ptr<ecs::ScriptState> LookupScriptGui(const std::string &windowName, const ecs::Scripts *scripts) {
        if (windowName.empty() && scripts) {
            for (auto &script : scripts->scripts) {
                if (!script.state) continue;
                ecs::ScriptState &state = *script.state;
                if (state.definition.type == ecs::ScriptType::GuiScript) {
                    if (std::holds_alternative<ecs::GuiRenderFuncs>(state.definition.callback)) {
                        return script.state;
                    } else {
                        Errorf("Gui script %s has invalid callback type: GuiScript != GuiRender",
                            state.definition.name);
                    }
                }
            }
        }
        if (!windowName.empty()) Errorf("unknown gui window: %s", windowName);
        return nullptr;
    }
} // namespace sp
