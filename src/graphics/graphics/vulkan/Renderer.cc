#include "Renderer.hh"

#include "GraphicsContext.hh"
#include "Vertex.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

// temporary for shader access, shaders should be compiled somewhere else later
#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Model.hh"

#include <tiny_gltf.h>

namespace sp::vulkan {
    Renderer::Renderer(ecs::Lock<ecs::AddRemove> lock, GraphicsContext &context)
        : context(context), device(context.Device()) {}

    Renderer::~Renderer() {
        device.waitIdle();
    }

    struct MeshPushConstants {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };

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
        Assert(vertShaderAsset != nullptr, "Test vertex shader asset is missing");
        vertShaderAsset->WaitUntilValid();
        vk::ShaderModuleCreateInfo vertShaderCreateInfo;
        vertShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(vertShaderAsset->Buffer());
        vertShaderCreateInfo.codeSize = vertShaderAsset->BufferSize();
        auto vertShaderModule = device.createShaderModuleUnique(vertShaderCreateInfo);

        auto fragShaderAsset = GAssets.Load("shaders/vulkan/bin/test.frag.spv");
        Assert(fragShaderAsset != nullptr, "Test fragment shader asset is missing");
        fragShaderAsset->WaitUntilValid();
        vk::ShaderModuleCreateInfo fragShaderCreateInfo;
        fragShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(fragShaderAsset->Buffer());
        fragShaderCreateInfo.codeSize = fragShaderAsset->BufferSize();
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

    class VulkanModel final : public NonCopyable {
    public:
        struct Primitive : public NonCopyable {
            UniqueBuffer indexBuffer;
            vk::IndexType indexType = vk::IndexType::eNoneKHR;
            size_t indexCount;

            UniqueBuffer vertexBuffer;
            glm::mat4 transform;
        };

        VulkanModel(const std::shared_ptr<const sp::Model> &model, Renderer *renderer)
            : renderer(renderer), modelName(model->name), sourceModel(model) {
            vector<SceneVertex> vertices;

            // TODO: cache the output somewhere. Keeping the conversion code in
            // the engine will be useful for any dynamic loading in the future,
            // but we don't want to do it every time a model is loaded.
            for (auto &primitive : model->Primitives()) {
                // TODO: this implementation assumes a lot about the model format,
                // and asserts the assumptions. It would be better to support more
                // kinds of inputs, and convert the data rather than just failing.
                Assert(primitive.drawMode == Model::DrawMode::Triangles, "draw mode must be Triangles");

                auto &p = *primitives.emplace_back(make_shared<Primitive>());
                auto &buffers = model->GetGltfModel()->buffers;

                switch (primitive.indexBuffer.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    p.indexType = vk::IndexType::eUint32;
                    Assert(primitive.indexBuffer.byteStride == 4, "index buffer must be tightly packed");
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    p.indexType = vk::IndexType::eUint16;
                    Assert(primitive.indexBuffer.byteStride == 2, "index buffer must be tightly packed");
                    break;
                }
                Assert(p.indexType != vk::IndexType::eNoneKHR, "unimplemented vertex index type");

                auto &indexBuffer = buffers[primitive.indexBuffer.bufferIndex];
                size_t indexBufferSize = primitive.indexBuffer.componentCount * primitive.indexBuffer.byteStride;
                Assert(primitive.indexBuffer.byteOffset + indexBufferSize <= indexBuffer.data.size(),
                       "indexes overflow buffer");

                p.indexBuffer = renderer->context.AllocateBuffer(indexBufferSize,
                                                                 vk::BufferUsageFlagBits::eIndexBuffer,
                                                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

                void *data;
                p.indexBuffer.Map(&data);
                memcpy(data, &indexBuffer.data[primitive.indexBuffer.byteOffset], indexBufferSize);
                p.indexBuffer.Unmap();

                p.indexCount = primitive.indexBuffer.componentCount;

                auto &posAttr = primitive.attributes[0];
                auto &normalAttr = primitive.attributes[1];
                auto &uvAttr = primitive.attributes[2];

                if (posAttr.componentCount) {
                    Assert(posAttr.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT,
                           "position attribute must be a float vector");
                    Assert(posAttr.componentFields == 3, "position attribute must be a vec3");
                }
                if (normalAttr.componentCount) {
                    Assert(normalAttr.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT,
                           "normal attribute must be a float vector");
                    Assert(normalAttr.componentFields == 3, "normal attribute must be a vec3");
                }
                if (uvAttr.componentCount) {
                    Assert(uvAttr.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT,
                           "uv attribute must be a float vector");
                    Assert(uvAttr.componentFields == 2, "uv attribute must be a vec2");
                }

                vertices.resize(posAttr.componentCount);

                for (size_t i = 0; i < posAttr.componentCount; i++) {
                    SceneVertex &vertex = vertices[i];

                    vertex.position = reinterpret_cast<const glm::vec3 &>(
                        buffers[posAttr.bufferIndex].data[posAttr.byteOffset + i * posAttr.byteStride]);

                    if (normalAttr.componentCount) {
                        vertex.normal = reinterpret_cast<const glm::vec3 &>(
                            buffers[normalAttr.bufferIndex].data[normalAttr.byteOffset + i * normalAttr.byteStride]);
                    }

                    if (uvAttr.componentCount) {
                        vertex.uv = reinterpret_cast<const glm::vec2 &>(
                            buffers[uvAttr.bufferIndex].data[uvAttr.byteOffset + i * uvAttr.byteStride]);
                    }
                }

                size_t vertexBufferSize = vertices.size() * sizeof(vertices[0]);
                p.vertexBuffer = renderer->context.AllocateBuffer(vertexBufferSize,
                                                                  vk::BufferUsageFlagBits::eVertexBuffer,
                                                                  VMA_MEMORY_USAGE_CPU_TO_GPU);

                p.vertexBuffer.Map(&data);
                memcpy(data, vertices.data(), vertexBufferSize);
                p.vertexBuffer.Unmap();
            }
        }

        ~VulkanModel() {
            Debugf("Destroying VulkanModel %s", modelName);
        }

        void AppendDrawCommands(vk::CommandBuffer &commands, glm::mat4 modelMat, const ecs::View &view) {
            for (auto &primitivePtr : primitives) {
                auto &primitive = *primitivePtr;
                MeshPushConstants constants;
                constants.projection = view.projMat;
                constants.view = view.viewMat;
                constants.model = modelMat * primitive.transform;

                commands.pushConstants(renderer->PipelineLayout(),
                                       vk::ShaderStageFlagBits::eVertex,
                                       0,
                                       sizeof(MeshPushConstants),
                                       &constants);

                commands.bindIndexBuffer(*primitive.indexBuffer, 0, primitive.indexType);
                commands.bindVertexBuffers(0, {*primitive.vertexBuffer}, {0});
                commands.drawIndexed(primitive.indexCount, 1, 0, 0, 0);
            }
        }

    private:
        vector<shared_ptr<Primitive>> primitives;
        Renderer *renderer;
        string modelName;

        std::shared_ptr<const sp::Model> sourceModel;
    };

    void Renderer::DrawEntity(vk::CommandBuffer &commands,
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
            model = std::make_shared<VulkanModel>(comp.model, this);
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
