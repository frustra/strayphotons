#include "Renderer.hh"

#include "CommandContext.hh"
#include "DeviceContext.hh"
#include "Model.hh"
#include "Vertex.hh"
#include "assets/Model.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

namespace sp::vulkan {
    Renderer::Renderer(ecs::Lock<ecs::AddRemove> lock, DeviceContext &device) : device(device) {
        viewStateUniformBuffer[0] = device.AllocateBuffer(sizeof(ViewStateUniforms),
                                                          vk::BufferUsageFlagBits::eUniformBuffer,
                                                          VMA_MEMORY_USAGE_CPU_TO_GPU);
        viewStateUniformBuffer[1] = device.AllocateBuffer(sizeof(ViewStateUniforms),
                                                          vk::BufferUsageFlagBits::eUniformBuffer,
                                                          VMA_MEMORY_USAGE_CPU_TO_GPU);
        viewStateUniformBuffer[2] = device.AllocateBuffer(sizeof(ViewStateUniforms),
                                                          vk::BufferUsageFlagBits::eUniformBuffer,
                                                          VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    Renderer::~Renderer() {
        device->waitIdle();
    }

    void Renderer::RenderPass(const CommandContextPtr &cmd, const ecs::View &view, DrawLock lock) {
        cmd->SetDefaultOpaqueState();

        cmd->SetShader(ShaderStage::Vertex, "test.vert");
        cmd->SetShader(ShaderStage::Fragment, "test.frag");

        ecs::View forwardPassView = view;
        ForwardPass(cmd, forwardPassView, lock, [&](auto lock, Tecs::Entity &ent) {});
    }

    void Renderer::ForwardPass(const CommandContextPtr &cmd,
                               ecs::View &view,
                               DrawLock lock,
                               const PreDrawFunc &preDraw) {
        static size_t foo = 0;
        foo = (foo + 1) % 3;
        ViewStateUniforms *viewState;
        viewStateUniformBuffer[foo]->Map((void **)&viewState);
        viewState->view[0] = view.viewMat;
        viewState->projection[0] = view.projMat;
        viewStateUniformBuffer[foo]->Unmap();

        cmd->SetUniformBuffer(0, 10, viewStateUniformBuffer[foo]);

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

    void Renderer::DrawEntity(const CommandContextPtr &cmd,
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

        model->AppendDrawCommands(cmd, modelMat); // TODO pass and use comp.model->bones
    }

    void Renderer::EndFrame() {
        activeModels.Tick();
    }
} // namespace sp::vulkan
