#pragma once

#include "Common.hh"
#include "Memory.hh"
#include "core/CFunc.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "graphics/core/RenderTarget.hh"

#include <functional>
#include <robin_hood.h>

namespace sp::vulkan {
    class Model;

    struct ViewStateUniforms {
        glm::mat4 view[2];
        glm::mat4 projection[2];
    };

    class Renderer {
    public:
        using DrawLock = typename ecs::Lock<ecs::Read<ecs::Renderable, ecs::View, ecs::Transform>>;
        typedef std::function<void(DrawLock, Tecs::Entity &)> PreDrawFunc;

        Renderer(ecs::Lock<ecs::AddRemove> lock, DeviceContext &context);
        ~Renderer();

        void Prepare() {}
        void BeginFrame(ecs::Lock<ecs::Read<ecs::Transform>> lock) {}
        void RenderPass(CommandContext &cmd, const ecs::View &view, DrawLock lock);
        void EndFrame();

        void ForwardPass(CommandContext &cmd, ecs::View &view, DrawLock lock, const PreDrawFunc &preDraw = {});

        void DrawEntity(CommandContext &cmd,
                        const ecs::View &view,
                        DrawLock lock,
                        Tecs::Entity &ent,
                        const PreDrawFunc &preDraw = {});

        float Exposure = 1.0f;

        DeviceContext &device;

    private:
        CFuncCollection funcs;

        BufferPtr viewStateUniformBuffer[3];

        PreservingMap<string, Model> activeModels;
    };
} // namespace sp::vulkan
