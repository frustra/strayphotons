#pragma once

#include "graphics/gui/GuiManager.hh"

namespace sp {
    class GraphicsManager;

    enum class MenuScreen { Splash, Main, Options, SceneSelect, Credits };

    enum class MenuRenderMode : int { None = 0, Pause = 1, Gel = 2 };

    class MenuGuiManager : public GuiManager {
    public:
        MenuGuiManager(GraphicsManager &graphics);
        virtual ~MenuGuiManager() {}

        void BeforeFrame() override;
        void DefineWindows() override;

        MenuRenderMode RenderMode() const;
        bool MenuOpen() const;
        void SetRenderMode(MenuRenderMode mode);

    private:
        GraphicsManager &graphics;

        MenuScreen selectedScreen = MenuScreen::Splash;

        float creditsScroll = 0.0f;
    };
} // namespace sp
