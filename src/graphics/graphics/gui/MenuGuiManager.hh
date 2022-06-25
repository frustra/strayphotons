#pragma once

#include "graphics/gui/SystemGuiManager.hh"

namespace sp {
    class GraphicsManager;
    class GpuTexture;

    enum class MenuScreen { Splash, Main, Options, SceneSelect, Credits };

    enum class MenuRenderMode : int { None = 0, Pause = 1, Gel = 2 };

    class MenuGuiManager : public SystemGuiManager {
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

        shared_ptr<GpuTexture> logoTex, frustraLogoTex;

        float creditsScroll = 0.0f;
    };
} // namespace sp
