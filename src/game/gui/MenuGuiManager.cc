#include "MenuGuiManager.hh"
#include "game/InputManager.hh"

#include <imgui/imgui.h>

namespace sp
{
	void MenuGuiManager::BindInput(InputManager &input)
	{
		SetGuiContext();
		ImGuiIO &io = ImGui::GetIO();
		inputManager = &input;

		io.MousePos = ImVec2(200, 200);

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

			if (state == GLFW_PRESS)
			{
				if (key == GLFW_KEY_ENTER && selectedScreen == 0)
				{
					selectedScreen = 1;
				}
			}
		});
	}

	void MenuGuiManager::BeforeFrame()
	{
		ImGuiIO &io = ImGui::GetIO();
		io.MouseDrawCursor = selectedScreen > 0;
		//Focused = true;
		//inputManager->LockFocus(true, FocusLevel);

		if (Focused && inputManager && inputManager->FocusLocked(FocusLevel))
		{
			auto &input = *inputManager;

			for (int i = 0; i < 3; i++)
			{
				io.MouseDown[i] = input.IsDown(MouseButtonToKey(i));
			}

			io.MouseWheel = input.ScrollOffset().y;

			auto cursorDiff = input.CursorDiff();
			io.MousePos.x = std::max(std::min(io.MousePos.x + cursorDiff.x, io.DisplaySize.x), 0.0f);
			io.MousePos.y = std::max(std::min(io.MousePos.y + cursorDiff.y, io.DisplaySize.y), 0.0f);
		}
	}

	void MenuGuiManager::DefineWindows()
	{
		ImGuiIO &io = ImGui::GetIO();

		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushFont(io.Fonts->Fonts[2]);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoTitleBar;

		if (selectedScreen == 0)
		{
			ImGui::SetNextWindowPosCenter(ImGuiSetCond_Always);

			ImGui::Begin("Menu", nullptr, flags);
			ImGui::Text("Press Enter");
			ImGui::End();
		}
		else if (selectedScreen == 1)
		{
		}

		ImGui::PopFont();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);
	}
}
