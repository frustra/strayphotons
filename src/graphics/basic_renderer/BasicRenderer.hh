#pragma once

#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"
#include "graphics/Renderer.hh"

#include <glm/glm.hpp>

namespace sp {
    class Game;

    class BasicRenderer : public Renderer {
    public:
        BasicRenderer(Game *game);
        ~BasicRenderer();

        void Prepare();
        void BeginFrame();
        void RenderPass(ecs::View view, RenderTarget::Ref finalOutput = nullptr);
        void PrepareForView(const ecs::View &view);
        void RenderLoading(ecs::View view);
        void EndFrame();

    private:
        void PrepareRenderable(const ecs::Renderable &comp);
        void DrawRenderable(const ecs::Renderable &comp);

        GLuint sceneProgram;
        std::map<Model::Primitive *, GLModel::Primitive> primitiveMap;
    };
} // namespace sp
