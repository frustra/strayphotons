#pragma once

#include <Common.hh>
#include <core/Game.hh>
#include <core/PerfTimer.hh>
#include <ecs/components/View.hh>
#include <graphics/RenderTarget.hh>
#include <graphics/Shader.hh>

namespace sp {
    class Game;

    class Renderer {
    public:
        Renderer(Game *g);
        virtual ~Renderer();

        virtual void Prepare() = 0;
        virtual void BeginFrame() = 0;
        virtual void RenderPass(ecs::View view, RenderTarget::Ref finalOutput = nullptr) = 0;
        virtual void PrepareForView(const ecs::View &view) = 0;
        virtual void RenderLoading(ecs::View view) = 0;
        virtual void EndFrame() = 0;

    public:
        // TODO: try to make these private
        ShaderSet *GlobalShaders;
        PerfTimer Timer;

    protected:
        Game *game;
    };
} // namespace sp
