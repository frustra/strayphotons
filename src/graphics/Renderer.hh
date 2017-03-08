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
	class GuiRenderer;

	struct VoxelData
	{
		shared_ptr<RenderTarget> fragmentList1;
		shared_ptr<RenderTarget> fragmentList2;
		shared_ptr<RenderTarget> packedData;
		shared_ptr<RenderTarget> radiance;
		ecs::VoxelInfo info;
	};

	class Renderer : public GraphicsContext
	{
	public:
		typedef std::function<void(ecs::Entity &)> PreDrawFunc;

		Renderer(Game *game);
		~Renderer();

		void UpdateShaders(bool force = false);
		void Prepare();
		void RenderMainMenu(ecs::View &view, bool renderToGel = false);
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
		void RenderLoading(ecs::View &view);
		void EndFrame();

		void SetRenderTarget(const Texture *attachment0, const Texture *depth);
		void SetRenderTargets(size_t attachmentCount, const Texture *attachments, const Texture *depth);
		void SetDefaultRenderTarget();

		ShaderManager *ShaderControl = nullptr;
		RenderTargetPool *RTPool = nullptr;

		float Exposure = 1.0f;
	private:
		shared_ptr<RenderTarget> shadowMap;
		shared_ptr<RenderTarget> mirrorShadowMap;
		shared_ptr<RenderTarget> menuGuiTarget;
		Buffer computeIndirectBuffer1, computeIndirectBuffer2;
		bool voxelFlipflop = true;
		VoxelData voxelData;
		Buffer mirrorVisData;
		Buffer mirrorSceneData;

		shared_ptr<GuiRenderer> debugGuiRenderer;
		shared_ptr<GuiRenderer> menuGuiRenderer;

		std::deque<std::pair<shared_ptr<Model>, int>> renderableGCQueue;
	};
}
