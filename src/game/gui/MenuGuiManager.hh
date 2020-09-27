#pragma once

#include "Common.hh"

#include <game/gui/GuiManager.hh>

namespace sp {
	class Game;
	class InputManager;

	enum class MenuScreen { Splash, Main, Options, SceneSelect, Credits };

	enum class MenuRenderMode : int { None = 0, Pause = 1, Gel = 2 };

	class MenuGuiManager : public GuiManager {
	public:
		MenuGuiManager(Game *game);
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

		uint64 framesSinceOpened = 0;
		float creditsScroll = 0.0f;
	};
} // namespace sp
