#pragma once

#include "graphics/gui/GuiManager.hh"

namespace sp {
    class GraphicsContext;

    class DebugGuiManager : public GuiManager {
    public:
        // TODO: Fix focus
        DebugGuiManager(GraphicsManager &graphics) : GuiManager(graphics /*, FOCUS_OVERLAY*/) {}
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
