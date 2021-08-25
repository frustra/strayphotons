#include "Renderer.hh"

#include "GraphicsContext.hh"
#include "Model.hh"
#include "Vertex.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

// temporary for shader access, shaders should be compiled somewhere else later
#include "assets/Asset.hh"
#include "assets/AssetManager.hh"

namespace sp::vulkan {
    Renderer::Renderer(ecs::Lock<ecs::AddRemove> lock, GraphicsContext &context)
        : context(context), device(context.Device()) {}

    Renderer::~Renderer() {
        device.waitIdle();
    }

    void Renderer::RenderPass(const ecs::View &view, DrawLock lock, RenderTarget *finalOutput) {
        uint32_t w = view.extents.x, h = view.extents.y;
        vk::Extent2D extent = {w, h};

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
        std::array<vk::ClearValue, 2> clearValues;
        clearValues[0].color = {colorArr};
        clearValues[1].depthStencil.depth = 1.0f;
        clearValues[1].depthStencil.stencil = 0.0f;

        renderPassInfo.clearValueCount = clearValues.size();
        renderPassInfo.pClearValues = clearValues.data();

        commands.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        commands.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

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
        depthImageView.reset();
        depthImage.Destroy();
        swapchainFramebuffers.clear();
        commandBuffers.clear();
        graphicsPipeline.reset();
        pipelineLayout.reset();
        renderPass.reset();
    }

    void Renderer::CreatePipeline(vk::Extent2D extent) {
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
        viewport.height = -(float)extent.height;
        viewport.x = 0;
        viewport.y = extent.height;
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
        rasterizer.frontFace = vk::FrontFace::eCounterClockwise;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        vk::PushConstantRange pushConstantRange;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(MeshPushConstants);
        pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        pipelineLayout = device.createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::ImageCreateInfo depthImageInfo;
        depthImageInfo.imageType = vk::ImageType::e2D;
        depthImageInfo.format = vk::Format::eD24UnormS8Uint;
        depthImageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        depthImageInfo.extent = vk::Extent3D(extent.width, extent.height, 1);
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.samples = vk::SampleCountFlagBits::e1;

        vk::AttachmentDescription colorAttachment;
        colorAttachment.format = context.SwapchainImageFormat();
        colorAttachment.samples = vk::SampleCountFlagBits::e1;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
        colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentDescription depthAttachment;
        depthAttachment.format = depthImageInfo.format;
        depthAttachment.samples = vk::SampleCountFlagBits::e1;
        depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
        depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::AttachmentReference colorAttachmentRef;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::AttachmentReference depthAttachmentRef;
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        vk::SubpassDependency dependency;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                  vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                  vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                   vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = attachments.size();
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        renderPass = device.createRenderPassUnique(renderPassInfo);

        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.sampleShadingEnable = false;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineDepthStencilStateCreateInfo depthStencil;
        depthStencil.depthTestEnable = true;
        depthStencil.depthWriteEnable = true;
        depthStencil.depthCompareOp = vk::CompareOp::eLess;
        depthStencil.depthBoundsTestEnable = false;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = false;

        auto vertexInputInfo = SceneVertex::InputInfo();

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo.PipelineInputInfo();
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = *pipelineLayout;
        pipelineInfo.renderPass = *renderPass;
        pipelineInfo.subpass = 0;

        auto pipelinesResult = device.createGraphicsPipelinesUnique({}, {pipelineInfo});
        AssertVKSuccess(pipelinesResult.result, "creating pipelines");
        graphicsPipeline = std::move(pipelinesResult.value[0]);

        depthImage = context.AllocateImage(depthImageInfo, VMA_MEMORY_USAGE_GPU_ONLY);

        vk::ImageViewCreateInfo depthImageViewInfo;
        depthImageViewInfo.format = depthImageInfo.format;
        depthImageViewInfo.image = *depthImage;
        depthImageViewInfo.viewType = vk::ImageViewType::e2D;
        depthImageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        depthImageViewInfo.subresourceRange.baseMipLevel = 0;
        depthImageViewInfo.subresourceRange.levelCount = 1;
        depthImageViewInfo.subresourceRange.baseArrayLayer = 0;
        depthImageViewInfo.subresourceRange.layerCount = 1;

        depthImageView = device.createImageViewUnique(depthImageViewInfo);

        auto imageViews = context.SwapchainImageViews();
        swapchainFramebuffers.clear();
        for (size_t i = 0; i < imageViews.size(); i++) {
            std::array<vk::ImageView, 2> attachments = {imageViews[i], *depthImageView};

            vk::FramebufferCreateInfo framebufferInfo{};
            framebufferInfo.renderPass = *renderPass;
            framebufferInfo.attachmentCount = attachments.size();
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            swapchainFramebuffers.push_back(device.createFramebufferUnique(framebufferInfo));
        }

        commandBuffers = context.CreateCommandBuffers(swapchainFramebuffers.size());
    }

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

        auto model = activeModels.Load(comp.model->name);
        if (!model) {
            model = make_shared<Model>(comp.model, this);
            activeModels.Register(comp.model->name, model);
        }

        model->AppendDrawCommands(commands, modelMat, view); // TODO pass and use comp.model->bones
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

        activeModels.Tick();
    }

} // namespace sp::vulkan
