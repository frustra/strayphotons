#pragma once

#include <glm/glm.hpp>
#include <vector>

class ImGuiContext;

namespace sp
{
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
}
