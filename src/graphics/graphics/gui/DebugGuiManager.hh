#pragma once

#include "graphics/gui/GuiManager.hh"

namespace sp {
    class GraphicsContext;

    class DebugGuiManager : public GuiManager {
    public:
        DebugGuiManager();
        virtual ~DebugGuiManager() {}

        void BeforeFrame() override;
        void DefineWindows() override;

    private:
        bool consoleOpen = false;
    };
} // namespace sp
