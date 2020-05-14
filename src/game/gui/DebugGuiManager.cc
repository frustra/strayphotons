#include "DebugGuiManager.hh"
#include "game/InputManager.hh"

#include <imgui/imgui.h>
#include "ConsoleGui.hh"

namespace sp
{
	void DebugGuiManager::DefineWindows()
	{
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

		static ConsoleGui console;
		if (consoleOpen) console.Add();
		GuiManager::DefineWindows();

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);
	}

	void DebugGuiManager::BeforeFrame()
	{
		ImGui::StyleColorsClassic();

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

	void DebugGuiManager::BindInput(InputManager &input)
	{
		SetGuiContext();
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

	void DebugGuiManager::GrabFocus()
	{
		if (!Focused())
		{
			inputManager->LockFocus(true, focusPriority);
			inputManager->EnableCursor();
		}
	}

	void DebugGuiManager::ReleaseFocus()
	{
		if (!Focused())
		{
			inputManager->DisableCursor();
			inputManager->LockFocus(false, focusPriority);
		}
	}

	void DebugGuiManager::ToggleConsole()
	{
		if (!consoleOpen)
			GrabFocus();

		consoleOpen = !consoleOpen;

		if (!consoleOpen)
			ReleaseFocus();
	}
}
