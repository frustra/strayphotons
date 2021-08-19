#include "GuiManager.hh"

#include "ConsoleGui.hh"
#include "input/InputManager.hh"

#include <imgui/imgui.h>

// clang-format off
// GLFW must be included after glew.h (Graphics.hh)
#include <GLFW/glfw3.h>
// clang-format on

namespace sp {
    GuiManager::GuiManager(GraphicsManager &graphics, const FocusLevel focusPriority)
        : focusPriority(focusPriority), graphics(graphics) {
        imCtx = ImGui::CreateContext();

        SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();

        // TODO: Set this mapping without requiring GLFW include.
        io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
        io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
        io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
        io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
        io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
        io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
        io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
        io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
        io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
        io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
        io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;
    }

    GuiManager::~GuiManager() {
        SetGuiContext();
        ImGui::DestroyContext(imCtx);
        imCtx = nullptr;
    }

    void GuiManager::SetGuiContext() {
        ImGui::SetCurrentContext(imCtx);
    }

    void GuiManager::BeforeFrame() {
        SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();

        // TODO: Use signals
        /*const KeyEvents *keys, *keysPrev;
        if (input.GetActionDelta(INPUT_ACTION_KEYBOARD_KEYS, &keys, &keysPrev)) {
            if (keysPrev != nullptr) {
                for (auto &key : *keysPrev) {
                    io.KeysDown[key] = false;
                }
            }
            for (auto &key : *keys) {
                io.KeysDown[key] = true;
            }

            io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
            io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
            io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
            io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
        };*/
    }

    void GuiManager::DefineWindows() {
        for (auto component : components) {
            component->Add();
        }
    }

    void GuiManager::Attach(GuiRenderable *component) {
        components.push_back(component);
    }
} // namespace sp
