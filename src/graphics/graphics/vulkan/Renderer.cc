#include "Renderer.hh"

#include "CommandContext.hh"
#include "DeviceContext.hh"
#include "Model.hh"
#include "Vertex.hh"
#include "assets/Model.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

namespace sp::vulkan {
    Renderer::Renderer(ecs::Lock<ecs::AddRemove> lock, DeviceContext &device) : device(device) {}

    Renderer::~Renderer() {
        device->waitIdle();
    }

    void Renderer::RenderPass(const ecs::View &view, DrawLock lock, RenderTarget *finalOutput) {
        // TODO: don't expose the image index, just return a temporary CommandContext from an internal pool
        auto swapchainImageIndex = device.CurrentSwapchainImageIndex();
        if (swapchainImageIndex >= commandContexts.size()) commandContexts.resize(swapchainImageIndex + 1);

        auto &commandsPtr = commandContexts[swapchainImageIndex];
        if (!commandsPtr) commandsPtr = device.CreateCommandContext();
        auto &commands = *commandsPtr;

        commands.BeginRenderPass(device.SwapchainRenderPassInfo(true));
        commands.SetDefaultOpaqueState();

        commands.SetShader(ShaderStage::Vertex, "test.vert");
        commands.SetShader(ShaderStage::Fragment, "test.frag");

        // TODO hook up view to renderpass

        ecs::View forwardPassView = view;
        ForwardPass(commands, forwardPassView, lock, [&](auto lock, Tecs::Entity &ent) {});

        commands.EndRenderPass();
    }

    void Renderer::ForwardPass(CommandContext &commands, ecs::View &view, DrawLock lock, const PreDrawFunc &preDraw) {
        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform>(lock)) {
                if (ent.Has<ecs::Mirror>(lock)) continue;
                DrawEntity(commands, view, lock, ent, preDraw);
            }
        }

        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform, ecs::Mirror>(lock)) {
                DrawEntity(commands, view, lock, ent, preDraw);
            }
        }
    }

    void Renderer::DrawEntity(CommandContext &commands,
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

        model->AppendDrawCommands(commands, modelMat, view); // TODO pass and use comp.model->bones
    }

    void Renderer::EndFrame() {
        vk::SubmitInfo submitInfo;
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

        auto imageAvailableSem = device.CurrentFrameImageAvailableSemaphore();
        auto renderCompleteSem = device.CurrentFrameRenderCompleteSemaphore();

        auto commands = commandContexts[device.CurrentSwapchainImageIndex()];
        const vk::CommandBuffer commandBuffer = commands->GetCommandBuffer();

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSem;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderCompleteSem;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        device.GraphicsQueue().submit({submitInfo}, device.ResetCurrentFrameFence());

        activeModels.Tick();
    }

} // namespace sp::vulkan
