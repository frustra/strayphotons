#pragma once

#include <game/gui/GuiManager.hh>

namespace sp
{
	class DebugGuiManager : public GuiManager
	{
	public:
		DebugGuiManager() : GuiManager(FOCUS_OVERLAY) { }
		virtual ~DebugGuiManager() { }

		void BeforeFrame();
		void DefineWindows();

		virtual void BindInput(GlfwInputManager *inputManager);

		bool Focused()
		{
			return consoleOpen;
		}
		void GrabFocus();
		void ReleaseFocus();

		void ToggleConsole();

	private:
		bool consoleOpen = false;
	};
}
