#include "GuiManager.hh"
#include "InputManager.hh"

#include <imgui/imgui.h>

namespace sp
{
	void GuiManager::DefineWindows()
	{
		ImGuiIO &io = ImGui::GetIO();

		if (consoleOpen)
		{
			ImGuiWindowFlags flags =
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoTitleBar;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::SetNextWindowPos(ImVec2(0, 0));

			ImGui::Begin("Console", nullptr, ImVec2(io.DisplaySize.x, 400.0f), 0.7f, flags);
			{
				ImGui::Text("hello world");
			}
			ImGui::End();

			ImGui::PopStyleVar();
		}
	}

	GuiManager::GuiManager()
	{
		ImGuiIO &io = ImGui::GetIO();
		io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
		io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
		io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
		io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
		io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
		io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
		io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
		io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
		io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
		io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
		io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
		io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
		io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
		io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;
	}

	void GuiManager::BeforeFrame()
	{
		ImGuiIO &io = ImGui::GetIO();
		io.MouseDrawCursor = Focused();

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
			if (ch > 0 && ch < 0x10000)
				io.AddInputCharacter(ch);

			if (ch == '`')
				ToggleConsole();
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
			inputManager->SetCursorPosition(guiCursorPos);
		}
	}

	void GuiManager::ReleaseFocus()
	{
		if (!Focused())
		{
			inputManager->LockFocus(false);
		}
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