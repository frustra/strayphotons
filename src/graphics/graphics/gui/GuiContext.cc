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
        ImGuiIO &io = ImGui::GetIO();

        io.KeyMap[ImGuiKey_Tab] = KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = KEY_LEFT_ARROW;
        io.KeyMap[ImGuiKey_RightArrow] = KEY_RIGHT_ARROW;
        io.KeyMap[ImGuiKey_UpArrow] = KEY_UP_ARROW;
        io.KeyMap[ImGuiKey_DownArrow] = KEY_DOWN_ARROW;
        io.KeyMap[ImGuiKey_PageUp] = KEY_PAGE_UP;
        io.KeyMap[ImGuiKey_PageDown] = KEY_PAGE_DOWN;
        io.KeyMap[ImGuiKey_Home] = KEY_HOME;
        io.KeyMap[ImGuiKey_End] = KEY_END;
        io.KeyMap[ImGuiKey_Delete] = KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Enter] = KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape] = KEY_ESCAPE;
        io.KeyMap[ImGuiKey_A] = KEY_A;
        io.KeyMap[ImGuiKey_C] = KEY_C;
        io.KeyMap[ImGuiKey_V] = KEY_V;
        io.KeyMap[ImGuiKey_X] = KEY_X;
        io.KeyMap[ImGuiKey_Y] = KEY_Y;
        io.KeyMap[ImGuiKey_Z] = KEY_Z;
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
