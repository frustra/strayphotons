#pragma once

#include "graphics/GraphicsContext.hh"
#include "graphics/RenderArgs.hh"

#include <glm/glm.hpp>

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
		void RenderFrame(RenderArgs args);

		void SetRenderTarget(const Texture *attachment0, const Texture *depth);
		void SetRenderTargets(size_t attachmentCount, const Texture *attachments, const Texture *depth);
		void SetDefaultRenderTarget();

		glm::mat4 GetView() const;
		glm::mat4 GetProjection() const;

		ShaderManager *ShaderControl = nullptr;
		RenderTargetPool *RTPool = nullptr;

	private:
		RenderArgs renderArgs;
	};
}
