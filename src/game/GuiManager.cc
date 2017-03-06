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
}
