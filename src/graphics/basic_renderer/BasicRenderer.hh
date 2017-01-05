#pragma once

#include "graphics/GraphicsContext.hh"

#include <glm/glm.hpp>

namespace sp
{
	class Game;

	class BasicRenderer : public GraphicsContext
	{
	public:
		BasicRenderer(Game *game);
		~BasicRenderer();

		void Prepare();
		void BeginFrame(ecs::View &fbView, int fullscreen);
		void RenderPass(ecs::View &view);
		void PrepareForView(ecs::View &view);
		void EndFrame();

	private:
		glm::ivec2 prevWindowSize, prevWindowPos;
		int prevFullscreen = 0;
		GLuint sceneProgram;
	};
}
