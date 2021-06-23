#pragma once

#include <game/gui/GuiManager.hh>

namespace sp {
    class GraphicsContext;
    class InputManager;

    class DebugGuiManager : public GuiManager {
    public:
        DebugGuiManager(GraphicsManager &graphics, InputManager &input) : GuiManager(graphics, input, FOCUS_OVERLAY) {}
        virtual ~DebugGuiManager() {}

        void BeforeFrame() override;
        void DefineWindows() override;

        bool Focused() {
            return consoleOpen;
        }

        void ToggleConsole();

    private:
        bool consoleOpen = false;
    };
} // namespace sp
