#ifndef SP_RENDERER_H
#define SP_RENDERER_H

#include "graphics/GraphicsContext.hh"
#include "graphics/ShaderManager.hh"
#include "core/Game.hh"
#include "assets/Model.hh"

namespace sp
{
	class Renderer : public GraphicsContext
	{
	public:
		Renderer(Game *game) : GraphicsContext(game)
		{
		}
		~Renderer();

		void Prepare();
		void RenderFrame();

	private:
		GLuint vertices, vertexAttribs, indexBuffer, texHandle;
		size_t numElems;

		ShaderManager *shaderManager = nullptr;
	};
}

#endif