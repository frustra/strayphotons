#pragma once

#include "graphics/GraphicsContext.hh"
#include "graphics/Texture.hh"

#include <glm/glm.hpp>

namespace sp
{
	class Game;
	class Renderer;
	class RenderTarget;

	namespace raytrace
	{
		class GPUSceneContext;

		class RaytracedRenderer
		{
		public:
			RaytracedRenderer(Game *game, Renderer *renderer);
			~RaytracedRenderer();

			bool Enable(ecs::View currView);
			void Disable();

			void Render();

		private:
			void ResetMaterialCache(GPUSceneContext &ctx);
			void ResetGeometryCache(GPUSceneContext &ctx);

			Game *game;
			Renderer *renderer;

			GPUSceneContext *gpuSceneCtx = nullptr;
			shared_ptr<RenderTarget> target;
			Texture baseColorRoughnessAtlas, normalMetalnessAtlas;

			glm::uvec2 invocationOffset;

			ecs::View view;
			bool enabled = false, cacheUpdating = false;
			float lastEV100 = 0.0f;
		};
	}
}
