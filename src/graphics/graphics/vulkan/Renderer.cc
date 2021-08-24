#include "Renderer.hh"

#include "GraphicsContext.hh"
#include "VertexBuffer.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/core/NativeModel.hh"

// temporary for shader access, shaders should be compiled somewhere else later
#include "assets/Asset.hh"
#include "assets/AssetManager.hh"

namespace sp::vulkan {
    struct TestVertex {
        glm::vec2 pos;
        glm::vec3 color;

        static VertexInputInfo InputInfo() {
            VertexInputInfo info(0, sizeof(TestVertex));
            info.PushAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(TestVertex, pos));
            info.PushAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(TestVertex, color));
            return info;
        }
    };

    std::vector<TestVertex> makeTestVertices() {
        static std::vector<TestVertex> vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                                   {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                                   {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};
        vertices[0].color.r += 0.001f;
        if (vertices[0].color.r > 1.0f) vertices[0].color.r = 0;
        return vertices;
    }

    Renderer::Renderer(ecs::Lock<ecs::AddRemove> lock, GraphicsContext &context)
        : context(context), device(context.Device()) {

        auto vertices = makeTestVertices();
        vertexBuffer = context.AllocateBuffer(sizeof(vertices[0]) * vertices.size(),
                                              vk::BufferUsageFlagBits::eVertexBuffer,
                                              VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    Renderer::~Renderer() {
        device.waitIdle();
    }

    void Renderer::RenderPass(const ecs::View &view, DrawLock lock, RenderTarget *finalOutput) {
        uint32_t w = view.extents.x, h = view.extents.y;
        vk::Extent2D extent = {w, h};

        auto vertices = makeTestVertices();
        void *data;
        vertexBuffer.Map(&data);
        memcpy(data, vertices.data(), vertexBuffer.Size());
        vertexBuffer.Unmap();

        if (pipelineSwapchainVersion != context.SwapchainVersion()) {
            // TODO hash input state and create pipeline when it changes, there are a lot more inputs than just the
            // swapchain. Also, we should be able to render pipelines that aren't bound to a swapchain image.
            CleanupPipeline();
            CreatePipeline(extent);
            pipelineSwapchainVersion = context.SwapchainVersion();
        }

        vk::CommandBufferBeginInfo beginInfo;
        auto &commands = *commandBuffers[context.CurrentSwapchainImageIndex()];
        commands.begin(beginInfo);

        vk::RenderPassBeginInfo renderPassInfo;
        renderPassInfo.renderPass = *renderPass;
        renderPassInfo.framebuffer = *swapchainFramebuffers[context.CurrentSwapchainImageIndex()];
        renderPassInfo.renderArea.extent = extent;

        // TODO hook up view to renderpass info
        std::array<float, 4> colorArr = {0.0f, 1.0f, 0.0f, 1.0f};
        vk::ClearColorValue color(colorArr);
        vk::ClearValue clearColor(color);
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        commands.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

        commands.bindVertexBuffers(0, {vertexBuffer.Get()}, {0});
        commands.draw(3, 1, 0, 0);

        ecs::View forwardPassView = view;
        ForwardPass(commands, forwardPassView, lock, [&](auto lock, Tecs::Entity &ent) {});

        commands.endRenderPass();
        commands.end();
    }

    void Renderer::ForwardPass(vk::CommandBuffer &commands,
                               ecs::View &view,
                               DrawLock lock,
                               const PreDrawFunc &preDraw) {
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

    void Renderer::CleanupPipeline() {
        swapchainFramebuffers.clear();
        commandBuffers.clear();
        graphicsPipeline.reset();
        pipelineLayout.reset();
        renderPass.reset();
    }

    void Renderer::CreatePipeline(vk::Extent2D extent) {
        // very temporary code to build a test pipeline

        auto vertShaderAsset = GAssets.Load("shaders/vulkan/bin/test.vert.spv");
        vk::ShaderModuleCreateInfo vertShaderCreateInfo;
        vertShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(vertShaderAsset->CharBuffer());
        vertShaderCreateInfo.codeSize = vertShaderAsset->buffer.size();
        auto vertShaderModule = device.createShaderModuleUnique(vertShaderCreateInfo);

        auto fragShaderAsset = GAssets.Load("shaders/vulkan/bin/test.frag.spv");
        vk::ShaderModuleCreateInfo fragShaderCreateInfo;
        fragShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(fragShaderAsset->CharBuffer());
        fragShaderCreateInfo.codeSize = fragShaderAsset->buffer.size();
        auto fragShaderModule = device.createShaderModuleUnique(fragShaderCreateInfo);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = *vertShaderModule;
        vertShaderStageInfo.pName = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = *fragShaderModule;
        fragShaderStageInfo.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

        vk::Viewport viewport;
        viewport.width = extent.width;
        viewport.height = extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor;
        scissor.extent = extent;

        vk::PipelineViewportStateCreateInfo viewportState;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.polygonMode = vk::PolygonMode::eFill;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = vk::CullModeFlagBits::eBack;
        rasterizer.frontFace = vk::FrontFace::eClockwise;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayout = device.createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::AttachmentDescription colorAttachment;
        colorAttachment.format = context.SwapchainImageFormat();
        colorAttachment.samples = vk::SampleCountFlagBits::e1;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
        colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference colorAttachmentRef;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        vk::SubpassDependency dependency;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        renderPass = device.createRenderPassUnique(renderPassInfo);

        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.sampleShadingEnable = false;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        auto vertexInputInfo = TestVertex::InputInfo();

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo.PipelineInputInfo();
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = *pipelineLayout;
        pipelineInfo.renderPass = *renderPass;
        pipelineInfo.subpass = 0;

        auto pipelinesResult = device.createGraphicsPipelinesUnique({}, {pipelineInfo});
        AssertVKSuccess(pipelinesResult.result, "creating pipelines");
        graphicsPipeline = std::move(pipelinesResult.value[0]);

        auto imageViews = context.SwapchainImageViews();
        swapchainFramebuffers.clear();
        for (size_t i = 0; i < imageViews.size(); i++) {
            vk::ImageView attachments[] = {imageViews[i]};

            vk::FramebufferCreateInfo framebufferInfo{};
            framebufferInfo.renderPass = *renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            swapchainFramebuffers.push_back(device.createFramebufferUnique(framebufferInfo));
        }

        commandBuffers = context.CreateCommandBuffers(swapchainFramebuffers.size());
    }

    class VulkanModel final : public NonCopyable, public NativeModel {
    public:
        VulkanModel(Model *model, Renderer *renderer) : NativeModel(model) {
            // for (auto &primitive : model->primitives) {}
        }

        void AppendDrawCommands(vk::CommandBuffer &commands, glm::mat4 modelMat, const ecs::View &view) {}
    };

    void Renderer::DrawEntity(vk::CommandBuffer &commands,
                              const ecs::View &view,
                              DrawLock lock,
                              Tecs::Entity &ent,
                              const PreDrawFunc &preDraw) {
        auto &comp = ent.Get<ecs::Renderable>(lock);

        // Filter entities that aren't members of all layers in the view's visibility mask.
        ecs::Renderable::VisibilityMask mask = comp.visibility;
        mask &= view.visibilityMask;
        if (mask != view.visibilityMask) return;

        glm::mat4 modelMat = ent.Get<ecs::Transform>(lock).GetGlobalTransform(lock);

        if (preDraw) preDraw(lock, ent);

        if (!comp.model->nativeModel) { comp.model->nativeModel = make_shared<VulkanModel>(comp.model.get(), this); };

        auto vkModel = static_cast<VulkanModel *>(comp.model->nativeModel.get());
        vkModel->AppendDrawCommands(commands, modelMat, view); // TODO pass and use comp.model->bones
    }

    void Renderer::EndFrame() {
        vk::SubmitInfo submitInfo;
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

        auto imageAvailableSem = context.CurrentFrameImageAvailableSemaphore();
        auto renderCompleteSem = context.CurrentFrameRenderCompleteSemaphore();

        auto buffer = *commandBuffers[context.CurrentSwapchainImageIndex()];

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSem;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderCompleteSem;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buffer;

        context.GraphicsQueue().submit({submitInfo}, context.ResetCurrentFrameFence());
    }

} // namespace sp::vulkan
