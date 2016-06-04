#pragma once

#include "graphics/GraphicsContext.hh"

namespace sp
{
	class Game;
	class RenderTargetPool;
	class ShaderManager;
	struct Texture;

	class Renderer : public GraphicsContext
	{
	public:
		Renderer(Game *game);
		~Renderer();

		void Prepare();
		void RenderFrame();

		void SetRenderTarget(const Texture *attachment0, const Texture *depth);
		void SetDefaultRenderTarget();

	private:
		ShaderManager *shaderManager = nullptr;
		RenderTargetPool *rtPool = nullptr;

		GLuint fb;
	};
}