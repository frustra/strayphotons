#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"
#include "graphics/core/RenderTarget.hh"
#include "graphics/opengl/PerfTimer.hh"
#include "graphics/opengl/Shader.hh"

namespace sp {
    class Renderer {
    public:
        Renderer(ecs::EntityManager &ecs);
        virtual ~Renderer();

        virtual void Prepare() = 0;
        virtual void BeginFrame() = 0;
        virtual void RenderPass(ecs::View view, RenderTarget *finalOutput = nullptr) = 0;
        virtual void PrepareForView(const ecs::View &view) = 0;
        virtual void RenderLoading(ecs::View view) = 0;
        virtual void EndFrame() = 0;

    public:
        // TODO: try to make these private
        ShaderSet *GlobalShaders;
        PerfTimer Timer;

    protected:
        ecs::EntityManager &ecs;
    };
} // namespace sp
