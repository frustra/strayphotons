#pragma once

#include "graphics/GraphicsContext.hh"
#include "graphics/Buffer.hh"
#include "graphics/Texture.hh"
#include "ecs/components/VoxelInfo.hh"

#include <glm/glm.hpp>
#include <functional>

namespace ecs
{
	class Entity;
}

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
		ecs::VoxelInfo info;
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
		void UpdateLightSensors();
		void BeginFrame();
		void RenderPass(ecs::View &view);
		void PrepareForView(ecs::View &view);
		void ForwardPass(ecs::View &view, SceneShader *shader, std::function<void(ecs::Entity &)> preDraw = [] (ecs::Entity &) {});
		void EndFrame();

		void SetRenderTarget(const Texture *attachment0, const Texture *depth);
		void SetRenderTargets(size_t attachmentCount, const Texture *attachments, const Texture *depth);
		void SetDefaultRenderTarget();

		ShaderManager *ShaderControl = nullptr;
		RenderTargetPool *RTPool = nullptr;

		Buffer mirrorVisData;
		float Exposure = 1.0f;
	private:
		shared_ptr<RenderTarget> shadowMap;
		shared_ptr<RenderTarget> mirrorShadowMap;
		Buffer computeIndirectBuffer;
		VoxelData voxelData;
	};
}
