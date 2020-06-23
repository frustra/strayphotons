#include "DebugGuiManager.hh"
#include <game/input/GlfwInputManager.hh>

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

		if (input != nullptr && Focused())
		{
			io.MouseDown[0] = input->IsDown(INPUT_ACTION_MOUSE_BASE + "/button_left");
			io.MouseDown[1] = input->IsDown(INPUT_ACTION_MOUSE_BASE + "/button_right");
			io.MouseDown[2] = input->IsDown(INPUT_ACTION_MOUSE_BASE + "/button_middle");

			glm::vec2 scrollOffset, scrollOffsetDelta;
			if (input->GetActionStateDelta(INPUT_ACTION_MOUSE_SCROLL, scrollOffset, scrollOffsetDelta))
			{
				io.MouseWheel = scrollOffsetDelta.y;
			}

			glm::vec2 guiCursorPos = input->ImmediateCursor();
			io.MousePos = ImVec2(guiCursorPos.x, guiCursorPos.y);
		}
	}

	void DebugGuiManager::BindInput(GlfwInputManager *inputManager)
	{
		GuiManager::BindInput(inputManager);

		if (input != nullptr)
		{
			ImGuiIO &io = ImGui::GetIO();

			input->AddCharInputCallback([&](uint32 ch)
			{
				if (ch == '`')
					ToggleConsole();
				else if (ch > 0 && ch < 0x10000)
					io.AddInputCharacter(ch);
			});
		}
	}

	void DebugGuiManager::GrabFocus()
	{
		if (input != nullptr && !Focused())
		{
			input->LockFocus(true, focusPriority);
			input->EnableCursor();
		}
	}

	void DebugGuiManager::ReleaseFocus()
	{
		if (input != nullptr && !Focused())
		{
			input->DisableCursor();
			input->LockFocus(false, focusPriority);
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
