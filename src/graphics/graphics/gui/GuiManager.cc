#include "GuiManager.hh"

#include "ConsoleGui.hh"
#include "ecs/EcsImpl.hh"
#include "input/core/BindingNames.hh"
#include "input/core/KeyCodes.hh"

#include <imgui/imgui.h>

namespace sp {
    GuiManager::GuiManager(GraphicsManager &graphics /*, const FocusLevel focusPriority*/)
        : /*focusPriority(focusPriority),*/ graphics(graphics) {
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

        playerEntity = ecs::NamedEntity("player");
        keyboardEntity = ecs::NamedEntity("keyboard");
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

        {
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::SignalBindings, ecs::SignalOutput>,
                                                    ecs::Write<ecs::EventInput>>();

            auto keyboard = keyboardEntity.Get(lock);
            if (keyboard.Has<ecs::SignalOutput>(lock)) {
                auto &signalOutput = keyboard.Get<ecs::SignalOutput>(lock);
                for (int keyCode = KEY_SPACE; keyCode < KEY_BACKTICK; keyCode++) {
                    auto keyName = KeycodeNameLookup.find(keyCode);
                    if (keyName != KeycodeNameLookup.end()) {
                        io.KeysDown[keyCode] = signalOutput.GetSignal(INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName->second);
                    }
                }
                for (int keyCode = KEY_ESCAPE; keyCode < KEY_RIGHT_SUPER; keyCode++) {
                    auto keyName = KeycodeNameLookup.find(keyCode);
                    if (keyName != KeycodeNameLookup.end()) {
                        io.KeysDown[keyCode] = signalOutput.GetSignal(INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName->second);
                    }
                }

                io.KeyCtrl = io.KeysDown[KEY_LEFT_CONTROL] || io.KeysDown[KEY_RIGHT_CONTROL];
                io.KeyShift = io.KeysDown[KEY_LEFT_SHIFT] || io.KeysDown[KEY_RIGHT_SHIFT];
                io.KeyAlt = io.KeysDown[KEY_LEFT_ALT] || io.KeysDown[KEY_RIGHT_ALT];
                io.KeySuper = io.KeysDown[KEY_LEFT_SUPER] || io.KeysDown[KEY_RIGHT_SUPER];
            }

            io.MouseWheel = 0.0f;
            io.MouseWheelH = 0.0f;
            auto player = playerEntity.Get(lock);
            if (player.Has<ecs::EventInput>(lock)) {
                ecs::Event event;
                while (ecs::EventInput::Poll(lock, player, INPUT_EVENT_MENU_SCROLL, event)) {
                    auto &scroll = std::get<glm::vec2>(event.data);
                    io.MouseWheel += scroll.y;
                    io.MouseWheelH += scroll.x;
                }
                while (ecs::EventInput::Poll(lock, player, INPUT_EVENT_MENU_TEXT_INPUT, event)) {
                    io.AddInputCharacter(std::get<char>(event.data));
                }
            }

            if (player.Has<ecs::SignalBindings>(lock)) {
                auto &bindings = player.Get<ecs::SignalBindings>(lock);
                io.MouseDown[0] = bindings.GetSignal(lock, INPUT_SIGNAL_MENU_BUTTON_LEFT) >= 0.5;
                io.MouseDown[1] = bindings.GetSignal(lock, INPUT_SIGNAL_MENU_BUTTON_RIGHT) >= 0.5;
                io.MouseDown[2] = bindings.GetSignal(lock, INPUT_SIGNAL_MENU_BUTTON_MIDDLE) >= 0.5;

                io.MousePos.x = bindings.GetSignal(lock, INPUT_SIGNAL_MENU_CURSOR_X);
                io.MousePos.y = bindings.GetSignal(lock, INPUT_SIGNAL_MENU_CURSOR_Y);
            }
        }
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
