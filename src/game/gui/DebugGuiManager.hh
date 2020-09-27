#pragma once

#include <game/gui/GuiManager.hh>

namespace sp {
	class Game;
	class InputManager;

	class DebugGuiManager : public GuiManager {
	public:
		DebugGuiManager(Game *game) : GuiManager(game, FOCUS_OVERLAY) {}
		virtual ~DebugGuiManager() {}

		void BeforeFrame() override;
		void DefineWindows() override;

		bool Focused() {
			return consoleOpen;
		}

		void ToggleConsole();

	private:
		bool consoleOpen = false;
	};
} // namespace sp
