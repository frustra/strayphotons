#pragma once

#include <glm/glm.hpp>
#include <vector>

struct ImGuiContext;

namespace sp
{
	/**
	 * Set of defined focus level priorities.
	 * A higher number has priority over lower numbers.
	 */
	enum FocusLevel
	{
		FOCUS_GAME = 1,
		FOCUS_MENU = 10,
		FOCUS_OVERLAY = 1000
	};

	class GlfwInputManager;

	class GuiRenderable
	{
	public:
		virtual void Add() = 0;
	};

	class GuiManager
	{
	public:
		GuiManager(const FocusLevel focusPriority = FOCUS_GAME);
		virtual ~GuiManager();
		virtual void BindInput(GlfwInputManager *inputManager);
		void Attach(GuiRenderable *component);
		void SetGuiContext();

		virtual void BeforeFrame() { }
		virtual void DefineWindows();

	protected:
		const FocusLevel focusPriority;
		GlfwInputManager *input = nullptr;

	private:
		std::vector<GuiRenderable *> components;
		ImGuiContext *imCtx = nullptr;
	};
}
