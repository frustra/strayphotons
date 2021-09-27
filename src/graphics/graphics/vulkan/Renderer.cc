#include "Renderer.hh"

#include "assets/Model.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Model.hh"
#include "graphics/vulkan/core/Vertex.hh"

namespace sp::vulkan {
    Renderer::Renderer(ecs::Lock<ecs::AddRemove> lock, DeviceContext &device) : device(device) {}

    Renderer::~Renderer() {
        device->waitIdle();
    }

    void Renderer::RenderPass(CommandContext &cmd, const ecs::View &view, DrawLock lock) {
        cmd.SetShader(ShaderStage::Vertex, "test.vert");
        cmd.SetShader(ShaderStage::Fragment, "test.frag");

        ecs::View forwardPassView = view;
        ForwardPass(cmd, forwardPassView, lock, [&](auto lock, Tecs::Entity &ent) {});
    }

    void Renderer::ForwardPass(CommandContext &cmd, ecs::View &view, DrawLock lock, const PreDrawFunc &preDraw) {
        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform>(lock)) {
                if (ent.Has<ecs::Mirror>(lock)) continue;
                DrawEntity(cmd, view, lock, ent, preDraw);
            }
        }

        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform, ecs::Mirror>(lock)) {
                DrawEntity(cmd, view, lock, ent, preDraw);
            }
        }
    }

    void Renderer::DrawEntity(CommandContext &cmd,
                              const ecs::View &view,
                              DrawLock lock,
                              Tecs::Entity &ent,
                              const PreDrawFunc &preDraw) {
        auto &comp = ent.Get<ecs::Renderable>(lock);
        if (!comp.model || !comp.model->Valid()) return;

        // Filter entities that aren't members of all layers in the view's visibility mask.
        ecs::Renderable::VisibilityMask mask = comp.visibility;
        mask &= view.visibilityMask;
        if (mask != view.visibilityMask) return;

        glm::mat4 modelMat = ent.Get<ecs::Transform>(lock).GetGlobalTransform(lock);

        if (preDraw) preDraw(lock, ent);

        auto model = activeModels.Load(comp.model->name);
        if (!model) {
            model = make_shared<Model>(*comp.model, device);
            activeModels.Register(comp.model->name, model);
        }

        model->Draw(cmd, modelMat); // TODO pass and use comp.model->bones
    }

    void Renderer::EndFrame() {
        activeModels.Tick();
    }
} // namespace sp::vulkan
