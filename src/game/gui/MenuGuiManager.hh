#pragma once

#include "game/gui/GuiManager.hh"

namespace sp {
    class GraphicsManager;
    class InputManager;

    enum class MenuScreen { Splash, Main, Options, SceneSelect, Credits };

    enum class MenuRenderMode : int { None = 0, Pause = 1, Gel = 2 };

    class MenuGuiManager : public GuiManager {
    public:
        MenuGuiManager(GraphicsManager &graphics, InputManager &input);
        virtual ~MenuGuiManager() {}

        void BeforeFrame() override;
        void DefineWindows() override;

        bool Focused();
        MenuRenderMode RenderMode();
        void SetRenderMode(MenuRenderMode mode);
        void OpenPauseMenu();
        void CloseMenu();

    private:
        MenuScreen selectedScreen = MenuScreen::Splash;

        uint64_t framesSinceOpened = 0;
        float creditsScroll = 0.0f;
    };
} // namespace sp
