#pragma once

#include "ecs/EventQueue.hh"
#include "graphics/gui/SystemGuiManager.hh"

namespace sp {
    class GraphicsManager;
    class GpuTexture;

    enum class MenuScreen { Main, Options, SceneSelect };

    class MenuGuiManager : public SystemGuiManager {
    public:
        MenuGuiManager(GraphicsManager &graphics);
        virtual ~MenuGuiManager() {}

        void BeforeFrame() override;
        void DefineWindows() override;

        bool MenuOpen() const;

    private:
        GraphicsManager &graphics;

        ecs::EventQueueRef events = ecs::NewEventQueue();

        MenuScreen selectedScreen = MenuScreen::Main;

        shared_ptr<GpuTexture> logoTex;
    };
} // namespace sp
