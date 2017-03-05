#pragma once

#include <glm/glm.hpp>
#include <vector>

class ImGuiContext;

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
		virtual ~GuiManager();
		void Attach(GuiRenderable *component);
		void SetGuiContext();

		virtual void BeforeFrame() { }
		virtual void DefineWindows();

	private:
		std::vector<GuiRenderable *> components;
		ImGuiContext *imCtx = nullptr;
	};

	class DebugGuiManager : public GuiManager
	{
	public:
		DebugGuiManager() { }
		virtual ~DebugGuiManager() { }

		void BeforeFrame();
		void DefineWindows();

		bool Focused()
		{
			return consoleOpen;
		}

		void BindInput(InputManager &inputManager);
		void GrabFocus();
		void ReleaseFocus();

		void ToggleConsole();

	private:
		InputManager *inputManager = nullptr;

		bool consoleOpen = false;
		glm::vec2 guiCursorPos = { 200.0f, 200.0f };
	};

	class MenuGuiManager : public GuiManager
	{
	public:
		MenuGuiManager() { }
		virtual ~MenuGuiManager() { }

		void BeforeFrame();
		void DefineWindows();
		void BindInput(InputManager &inputManager);

	private:
		InputManager *inputManager = nullptr;
	};
}
