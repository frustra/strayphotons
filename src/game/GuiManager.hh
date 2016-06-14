#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace sp
{
	class InputManager;

	class GuiRenderable
	{
	public:
		virtual void Add() = 0;
	};

	class GuiManager
	{
	public:
		GuiManager();
		void BeforeFrame();
		void DefineWindows();

		bool Focused()
		{
			return consoleOpen;
		}

		void BindInput(InputManager &inputManager);
		void GrabFocus();
		void ReleaseFocus();

		void Attach(GuiRenderable *component);

		void ToggleConsole();

	private:
		InputManager *inputManager = nullptr;

		bool consoleOpen = false;
		glm::vec2 guiCursorPos = { 200.0f, 200.0f };

		std::vector<GuiRenderable *> components;
	};
}