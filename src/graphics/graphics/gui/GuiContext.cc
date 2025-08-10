/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GuiContext.hh"

#include "ecs/EcsImpl.hh"
#include "graphics/gui/definitions/ConsoleGui.hh"
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

    void GuiContext::SetGuiContext() {
        ImGui::SetCurrentContext(imCtx);
    }

    void GuiContext::BeforeFrame() {
        SetGuiContext();
    }

    void GuiContext::Attach(const Ref &component) {
        if (!sp::contains(components, component)) components.emplace_back(component);
    }

    void GuiContext::Detach(const Ref &component) {
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end()) components.erase(it);
    }

    GuiContext::Ref CreateGuiWindow(const ecs::Gui &gui, const ecs::Scripts *scripts) {
        if (gui.windowName == "lobby") {
            static const auto lobby = make_shared<LobbyGui>(gui.windowName);
            return lobby;
        } else if (gui.windowName == "entity_picker") {
            static const auto entityPicker = make_shared<EntityPickerGui>(gui.windowName);
            return entityPicker;
        } else if (gui.windowName == "inspector") {
            static const auto inspector = make_shared<InspectorGui>(gui.windowName);
            return inspector;
        } else if (gui.windowName == "signal_display") {
            static const auto signalDisplay = make_shared<SignalDisplayGui>(gui.windowName);
            return signalDisplay;
        } else if (gui.windowName.empty() && scripts) {
            for (auto &script : scripts->scripts) {
                if (!script.state) continue;
                ecs::ScriptState &state = *script.state;
                if (state.definition.type == ecs::ScriptType::GuiScript) {
                    Assertf(std::holds_alternative<ecs::GuiRenderableFunc>(state.definition.callback),
                        "Gui script %s has invalid callback type: GuiScript != GuiRenderable",
                        state.definition.name);
                    auto &getGuiRenderable = std::get<ecs::GuiRenderableFunc>(state.definition.callback);
                    return getGuiRenderable(state);
                }
            }
        }
        if (!gui.windowName.empty()) Errorf("unknown gui window: %s", gui.windowName);
        return std::weak_ptr<ecs::GuiRenderable>();
    }
} // namespace sp
