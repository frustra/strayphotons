#include "GuiManager.hh"
#include "InputManager.hh"

#include <imgui/imgui.h>
#include "gui/ConsoleGui.hh"

namespace sp
{
	void GuiManager::DefineWindows()
	{
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

		static ConsoleGui console;
		if (consoleOpen) console.Add();

		for (auto component : components)
		{
			component->Add();
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);
	}

	GuiManager::GuiManager()
	{
	}

	void GuiManager::BeforeFrame()
	{
		ImGuiIO &io = ImGui::GetIO();
		io.MouseDrawCursor = false;

		if (inputManager && Focused())
		{
			auto &input = *inputManager;

			for (int i = 0; i < 3; i++)
			{
				io.MouseDown[i] = input.IsDown(MouseButtonToKey(i));
			}

			io.MouseWheel = input.ScrollOffset().y;

			guiCursorPos = input.ImmediateCursor();
			io.MousePos = ImVec2(guiCursorPos.x, guiCursorPos.y);
		}
	}

	void GuiManager::BindInput(InputManager &input)
	{
		ImGuiIO &io = ImGui::GetIO();
		inputManager = &input;

		input.AddCharInputCallback([&](uint32 ch)
		{
			if (ch == '`')
				ToggleConsole();
			else if (ch > 0 && ch < 0x10000)
				io.AddInputCharacter(ch);
		});

		input.AddKeyInputCallback([&](int key, int state)
		{
			if (state == GLFW_PRESS)
				io.KeysDown[key] = true;
			if (state == GLFW_RELEASE)
				io.KeysDown[key] = false;

			io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
			io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
			io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
			io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
		});
	}

	void GuiManager::GrabFocus()
	{
		if (!Focused())
		{
			inputManager->LockFocus(true);
			inputManager->EnableCursor();
		}
	}

	void GuiManager::ReleaseFocus()
	{
		if (!Focused())
		{
			inputManager->DisableCursor();
			inputManager->LockFocus(false);
		}
	}

	void GuiManager::Attach(GuiRenderable *component)
	{
		components.push_back(component);
	}

	void GuiManager::ToggleConsole()
	{
		if (!consoleOpen)
			GrabFocus();

		consoleOpen = !consoleOpen;

		if (!consoleOpen)
			ReleaseFocus();
	}
}
