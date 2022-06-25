#pragma once

#include "graphics/gui/SystemGuiManager.hh"

namespace sp {
    class GraphicsContext;

    class DebugGuiManager : public SystemGuiManager {
    public:
        DebugGuiManager();
        virtual ~DebugGuiManager() {}

        void BeforeFrame() override;
        void DefineWindows() override;

    private:
        bool consoleOpen = false;
    };
} // namespace sp
