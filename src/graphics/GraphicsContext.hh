#ifndef SP_GRAPHICSCONTEXT_H
#define SP_GRAPHICSCONTEXT_H

#include <string>

#include "graphics/Graphics.hh"
#include "Common.hh"

namespace sp
{
	class Device;
	class ShaderSet;
	class Game;

	class GraphicsContext
	{
	public:
		GraphicsContext(Game *game);
		virtual ~GraphicsContext();

		void CreateWindow();
		bool ShouldClose();
		void SetTitle(string title);
		void ResetSwapchain(uint32 &width, uint32 &height);

		virtual void Prepare() = 0;
		virtual void RenderFrame() = 0;

		ShaderSet *ShaderSet;

	private:

	protected:
		GLFWwindow *window = nullptr;
		Game *game;
	};
}

#endif

