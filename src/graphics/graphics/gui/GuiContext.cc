#include "GuiContext.hh"

#include "graphics/gui/definitions/ConsoleGui.hh"
#include "graphics/gui/definitions/InspectorGui.hh"
#include "graphics/gui/definitions/LobbyGui.hh"
#include "input/KeyCodes.hh"

#include <imgui/imgui.h>

namespace sp {
    static std::array fontList = {
        FontDef{Font::Primary, "DroidSans.ttf", 16.0f},
        FontDef{Font::Primary, "DroidSans.ttf", 32.0f},
        FontDef{Font::Monospace, "3270Medium.ttf", 25.0f},
        FontDef{Font::Monospace, "3270Medium.ttf", 32.0f},
    };

    std::span<FontDef> GetFontList() {
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

    shared_ptr<GuiWindow> CreateGuiWindow(const string &windowName) {
        shared_ptr<GuiWindow> window;
        if (windowName == "lobby") {
            window = make_shared<LobbyGui>(windowName);
        } else if (windowName == "inspector") {
            window = make_shared<InspectorGui>(windowName);
        } else {
            Errorf("unknown gui window: %s", windowName);
            return nullptr;
        }
        return window;
    }

    static void pushFont(Font fontType, float fontSize) {
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

    void GuiRenderable::PushFont(Font fontType, float fontSize) {
        pushFont(fontType, fontSize);
    }

    void GuiContext::PushFont(Font fontType, float fontSize) {
        pushFont(fontType, fontSize);
    }
} // namespace sp
