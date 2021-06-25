#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"

namespace sp {
    class ShaderSet;
    class RenderTarget;

    class Renderer {
    public:
        Renderer(ecs::EntityManager &ecs) : ecs(ecs) {}
        virtual ~Renderer() {}

        virtual void Prepare() = 0;
        virtual void BeginFrame() = 0;
        virtual void RenderPass(ecs::View view, RenderTarget *finalOutput = nullptr) = 0;
        virtual void PrepareForView(const ecs::View &view) = 0;
        virtual void RenderLoading(ecs::View view) = 0;
        virtual void EndFrame() = 0;

    protected:
        ecs::EntityManager &ecs;
    };
} // namespace sp
