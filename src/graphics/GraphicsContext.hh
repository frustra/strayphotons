#pragma once

#include <string>

#include "graphics/Graphics.hh"
#include "ecs/components/View.hh"
#include "Common.hh"

namespace sp
{
	class Device;
	class ShaderSet;
	class Game;
	class InputManager;

	class GraphicsContext
	{
	public:
		GraphicsContext(Game *game);
		virtual ~GraphicsContext();

		void CreateWindow();
		bool ShouldClose();
		void SetTitle(string title);
		void BindInputCallbacks(InputManager &inputManager);

		virtual void Prepare() = 0;
		virtual void RenderPass(ecs::View &view) = 0;
		virtual void EndFrame() = 0;

		ShaderSet *GlobalShaders;

		GLFWwindow *GetWindow()
		{
			return window;
		}

	private:

	protected:
		GLFWwindow *window = nullptr;
		Game *game;
	};
}
