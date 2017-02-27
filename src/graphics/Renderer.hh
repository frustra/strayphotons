#pragma once

#include "graphics/GraphicsContext.hh"
#include "graphics/Buffer.hh"
#include "graphics/Texture.hh"
#include "ecs/components/VoxelInfo.hh"

#include <glm/glm.hpp>
#include <functional>
#include <Ecs.hh>

namespace sp
{
	class Game;
	class RenderTarget;
	class RenderTargetPool;
	class ShaderManager;
	class SceneShader;
	class Model;

	struct VoxelData
	{
		shared_ptr<RenderTarget> fragmentList;
		shared_ptr<RenderTarget> packedData;
		shared_ptr<RenderTarget> color, normal, radiance;
		ecs::VoxelInfo info;
		bool voxelsCleared = true;
	};

	class Renderer : public GraphicsContext
	{
	public:
		typedef std::function<void(ecs::Entity &)> PreDrawFunc;

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
		void ForwardPass(ecs::View &view, SceneShader *shader, const PreDrawFunc &preDraw = {});
		void DrawEntity(ecs::View &view, SceneShader *shader, ecs::Entity &ent, const PreDrawFunc &preDraw = {});
		void EndFrame();

		void SetRenderTarget(const Texture *attachment0, const Texture *depth);
		void SetRenderTargets(size_t attachmentCount, const Texture *attachments, const Texture *depth);
		void SetDefaultRenderTarget();

		ShaderManager *ShaderControl = nullptr;
		RenderTargetPool *RTPool = nullptr;

		// TODO(jli): private
		Buffer mirrorVisData;
		Buffer mirrorSceneData;
		float Exposure = 1.0f;
	private:
		shared_ptr<RenderTarget> shadowMap;
		shared_ptr<RenderTarget> mirrorShadowMap;
		Buffer computeIndirectBuffer;
		VoxelData voxelData;

		std::deque<std::pair<shared_ptr<Model>, int>> renderableGCQueue;
	};
}
