#include "GuiManager.hh"
#include <game/input/InputManager.hh>
#include <game/input/GlfwInputManager.hh>

#include <imgui/imgui.h>
#include "ConsoleGui.hh"

// clang-format off
// GLFW must be included after glew.h (Graphics.hh)
#include <GLFW/glfw3.h>
// clang-format on

namespace sp
{
	GuiManager::GuiManager(const FocusLevel focusPriority) : focusPriority(focusPriority)
	{
		imCtx = ImGui::CreateContext();
	}

	GuiManager::~GuiManager()
	{
		SetGuiContext();
		ImGui::DestroyContext(imCtx);
		imCtx = nullptr;
	}

	void GuiManager::BindInput(GlfwInputManager *inputManager)
	{
		Assert(input == nullptr, "InputManager can only be bound once.");

		if (inputManager != nullptr)
		{
			input = inputManager;

			SetGuiContext();
			ImGuiIO &io = ImGui::GetIO();
			io.MousePos = ImVec2(200, 200);

			input->AddKeyInputCallback([&](int key, int state)
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
}
