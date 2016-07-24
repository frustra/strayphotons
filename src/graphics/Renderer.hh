#pragma once

#include "graphics/GraphicsContext.hh"

#include <glm/glm.hpp>

namespace sp
{
	class Game;
	class RenderTarget;
	class RenderTargetPool;
	class ShaderManager;
	class SceneShader;
	class GPUTimer;
	struct Texture;

	class Renderer : public GraphicsContext
	{
	public:
		Renderer(Game *game);
		~Renderer();

		void Prepare();
		shared_ptr<RenderTarget> RenderShadowMaps();
		void BeginFrame(ecs::View &fbView, int fullscreen);
		void RenderPass(ecs::View &view, shared_ptr<RenderTarget> shadowMap);
		void PrepareForView(ecs::View &view);
		void ForwardPass(ecs::View &view, SceneShader *shader);
		void EndFrame();

		void SetRenderTarget(const Texture *attachment0, const Texture *depth);
		void SetRenderTargets(size_t attachmentCount, const Texture *attachments, const Texture *depth);
		void SetDefaultRenderTarget();

		ShaderManager *ShaderControl = nullptr;
		RenderTargetPool *RTPool = nullptr;
		GPUTimer *timer = nullptr;

	private:
		glm::ivec2 prevWindowSize, prevWindowPos;
		int prevFullscreen = 0;
	};
}
