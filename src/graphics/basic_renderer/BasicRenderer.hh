#pragma once

#include "graphics/GraphicsContext.hh"
#include "ecs/components/Renderable.hh"
#include "assets/Model.hh"

#include <glm/glm.hpp>

namespace sp
{
	class Game;

	class BasicRenderer : public GraphicsContext
	{
	public:
		BasicRenderer(Game *game);
		~BasicRenderer();

		void Prepare();
		void BeginFrame();
		void RenderPass(ecs::View &view);
		void PrepareForView(ecs::View &view);
		void RenderLoading(ecs::View &view);
		void EndFrame();

	private:
		void PrepareRenderable(ecs::Handle<ecs::Renderable> comp);
		void DrawRenderable(ecs::Handle<ecs::Renderable> comp);

		GLuint sceneProgram;
		std::map<Model::Primitive *, GLModel::Primitive> primitiveMap;
	};
}
