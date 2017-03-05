#include "GuiManager.hh"
#include "InputManager.hh"

#include <imgui/imgui.h>
#include "gui/ConsoleGui.hh"

namespace sp
{
	GuiManager::GuiManager()
	{
		imCtx = ImGui::CreateContext();
	}

	GuiManager::~GuiManager()
	{
		SetGuiContext();
		ImGui::Shutdown();
		ImGui::DestroyContext(imCtx);
		imCtx = nullptr;
	}

	void GuiManager::SetGuiContext()
	{
		ImGui::SetCurrentContext(imCtx);
	}

	void GuiManager::DefineWindows()
	{
		for (auto component : components)
		{
			component->Add();
		}
	}

	void GuiManager::Attach(GuiRenderable *component)
	{
		components.push_back(component);
	}

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
			inputManager->LockFocus(true);
			inputManager->EnableCursor();
		}
	}

	void DebugGuiManager::ReleaseFocus()
	{
		if (!Focused())
		{
			inputManager->DisableCursor();
			inputManager->LockFocus(false);
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

	void MenuGuiManager::BindInput(InputManager &input)
	{
		SetGuiContext();
		ImGuiIO &io = ImGui::GetIO();
		inputManager = &input;

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

	void MenuGuiManager::BeforeFrame()
	{
		ImGuiIO &io = ImGui::GetIO();
		io.MouseDrawCursor = false;
	}

	void MenuGuiManager::DefineWindows()
	{
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

		static ConsoleGui console;
		console.Add();
		GuiManager::DefineWindows();

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);
	}
}
