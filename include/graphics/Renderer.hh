#ifndef SP_RENDERER_H
#define SP_RENDERER_H

#include "graphics/GraphicsContext.hh"
#include "graphics/ShaderManager.hh"

namespace sp
{
	class Renderer : public GraphicsContext
	{
	public:
		Renderer()
		{
		}
		~Renderer();

		void Prepare();
		void RenderFrame();

	private:
		GLuint vertices, vertexAttribs, indexBuffer;

		ShaderManager *shaderManager = nullptr;
	};
}

#endif