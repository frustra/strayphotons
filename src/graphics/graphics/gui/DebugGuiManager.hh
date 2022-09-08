#pragma once

#include "ecs/Ecs.hh"
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
        void AddGui(ecs::Entity ent, const ecs::Gui &gui);

        bool consoleOpen = false;
        ecs::ComponentObserver<ecs::Gui> guiObserver;

        struct GuiEntityContext {
            ecs::Entity entity;
            shared_ptr<GuiWindow> window = nullptr;
        };
        vector<GuiEntityContext> guis;
    };
} // namespace sp
