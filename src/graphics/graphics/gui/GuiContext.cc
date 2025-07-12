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

    void GuiContext::Attach(const std::shared_ptr<GuiRenderable> &component) {
        if (!sp::contains(components, component)) components.emplace_back(component);
    }

    void GuiContext::Detach(const std::shared_ptr<GuiRenderable> &component) {
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end()) components.erase(it);
    }

    shared_ptr<GuiRenderable> CreateGuiWindow(const string &windowName, const ecs::Entity &ent) {
        shared_ptr<GuiRenderable> window;
        if (windowName == "lobby") {
            window = make_shared<LobbyGui>(windowName);
        } else if (windowName == "entity_picker") {
            window = make_shared<EntityPickerGui>(windowName);
        } else if (windowName == "inspector") {
            window = make_shared<InspectorGui>(windowName);
        } else if (windowName == "signal_display") {
            window = make_shared<SignalDisplayGui>(windowName, ent);
        } else {
            Errorf("unknown gui window: %s", windowName);
            return nullptr;
        }
        return window;
    }

    static void pushFont(GuiFont fontType, float fontSize) {
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

    void GuiRenderable::PushFont(GuiFont fontType, float fontSize) {
        pushFont(fontType, fontSize);
    }

    void GuiContext::PushFont(GuiFont fontType, float fontSize) {
        pushFont(fontType, fontSize);
    }
} // namespace sp
