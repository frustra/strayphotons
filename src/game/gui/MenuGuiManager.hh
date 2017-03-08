#pragma once

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
		SceneSelect
	};

	class MenuGuiManager : public GuiManager
	{
	public:
		MenuGuiManager(Game *game) : game(game) { }
		virtual ~MenuGuiManager() { }

		void BeforeFrame();
		void DefineWindows();
		void BindInput(InputManager &inputManager);

		bool Focused = false;

		const int FocusLevel = 10;

	private:
		Game *game = nullptr;
		InputManager *inputManager = nullptr;
		MenuScreen selectedScreen = MenuScreen::Splash;
	};
}
