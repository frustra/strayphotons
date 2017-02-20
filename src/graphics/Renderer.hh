#pragma once

#include "graphics/GraphicsContext.hh"
#include "graphics/Buffer.hh"
#include "graphics/Texture.hh"
#include "ecs/components/VoxelInfo.hh"

#include <glm/glm.hpp>

namespace sp
{
	class Game;
	class RenderTarget;
	class RenderTargetPool;
	class ShaderManager;
	class SceneShader;

	struct VoxelData
	{
		shared_ptr<RenderTarget> fragmentList;
		shared_ptr<RenderTarget> packedData;
		shared_ptr<RenderTarget> color, normal, radiance;
		bool mipmapsGenerated;
	};

	class Renderer : public GraphicsContext
	{
	public:
		Renderer(Game *game);
		~Renderer();

		void Prepare();
		void RenderShadowMaps();
		void PrepareVoxelTextures();
		void RenderVoxelGrid();
		void ClearVoxelGrid();
		void BeginFrame(ecs::View &fbView, int fullscreen);
		void RenderPass(ecs::View &view);
		void PrepareForView(ecs::View &view);
		void ForwardPass(ecs::View &view, SceneShader *shader);
		void EndFrame();

		void SetRenderTarget(const Texture *attachment0, const Texture *depth);
		void SetRenderTargets(size_t attachmentCount, const Texture *attachments, const Texture *depth);
		void SetDefaultRenderTarget();

		ShaderManager *ShaderControl = nullptr;
		RenderTargetPool *RTPool = nullptr;

		ecs::VoxelInfo voxelInfo;
	private:
		shared_ptr<RenderTarget> shadowMap;
		shared_ptr<RenderTarget> mirrorData;
		Buffer computeIndirectBuffer;
		VoxelData voxelData;
	};
}
