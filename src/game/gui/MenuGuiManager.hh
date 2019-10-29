#pragma once

#include "Common.hh"
#include "game/GuiManager.hh"

namespace sp
{
	class InputManager;
	class Game;

	enum class MenuScreen
	{
		Splash,
		Main,
		Options,
		SceneSelect,
		Credits
	};

	enum class MenuRenderMode : int
	{
		None = 0,
		Pause = 1,
		Gel = 2
	};

	class MenuGuiManager : public GuiManager
	{
	public:
		MenuGuiManager(Game *game) : GuiManager(FOCUS_MENU), game(game) { }
		virtual ~MenuGuiManager() { }

		void BeforeFrame();
		void DefineWindows();
		void BindInput(InputManager &inputManager);

		bool Focused();
		MenuRenderMode RenderMode();
		void SetRenderMode(MenuRenderMode mode);
		void OpenPauseMenu();
		void CloseMenu();

	private:
		Game *game = nullptr;
		InputManager *inputManager = nullptr;
		MenuScreen selectedScreen = MenuScreen::Splash;

		uint64 framesSinceOpened = 0;
		float creditsScroll = 0.0f;
	};
}
