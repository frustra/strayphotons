#pragma once

#include "game/GuiManager.hh"

namespace sp
{
	class InputManager;

	class MenuGuiManager : public GuiManager
	{
	public:
		virtual ~MenuGuiManager() { }

		void BeforeFrame();
		void DefineWindows();
		void BindInput(InputManager &inputManager);

		bool Focused = false;

		const int FocusLevel = 10;

	private:
		InputManager *inputManager = nullptr;
		int selectedScreen = 0;
	};
}
