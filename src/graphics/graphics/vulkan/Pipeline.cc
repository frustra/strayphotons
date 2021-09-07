#include "Pipeline.hh"

#include "DeviceContext.hh"
#include "Vertex.hh"

namespace sp::vulkan {

    PipelineManager::PipelineManager(DeviceContext &device) : device(device) {
        vk::PipelineCacheCreateInfo pipelineCacheInfo;
        pipelineCache = device->createPipelineCacheUnique(pipelineCacheInfo);
    }

    ShaderSet FetchShaders(const DeviceContext &device, const ShaderHandleSet &handles) {
        ShaderSet shaders;
        for (size_t i = 0; i < (size_t)ShaderStage::Count; i++) {
            ShaderHandle handle = handles[i];
            if (handle) shaders[i] = device.GetShader(handle);
        }
        return shaders;
    }

    ShaderHashSet GetShaderHashes(const ShaderSet &shaders) {
        ShaderHashSet hashes;
        for (size_t i = 0; i < (size_t)ShaderStage::Count; i++) {
            auto &shader = shaders[i];
            hashes[i] = shader ? shaders[i]->hash : 0;
        }
        return hashes;
    }

    shared_ptr<PipelineLayout> PipelineManager::GetPipelineLayout(const ShaderSet &shaders,
                                                                  const vk::DescriptorSetLayout &descriptorSetLayout) {
        PipelineLayoutKey key;
        key.input.shaderHashes = GetShaderHashes(shaders);
        key.input.descriptorSetLayout = descriptorSetLayout;

        auto &mapValue = pipelineLayouts[key];
        if (!mapValue) { mapValue = make_shared<PipelineLayout>(device, shaders, descriptorSetLayout); }
        return mapValue;
    }

    PipelineLayout::PipelineLayout(DeviceContext &device,
                                   const ShaderSet &shaders,
                                   const vk::DescriptorSetLayout &descriptorSetLayout) {
        uint32 count;

        // TODO: implement other fields on vk::PipelineLayoutCreateInfo
        // They should all be possible to determine with shader reflection.

        for (size_t stageIndex = 0; stageIndex < (size_t)ShaderStage::Count; stageIndex++) {
            auto &shader = shaders[stageIndex];
            if (!shader) continue;

            auto stage = ShaderStageToFlagBits[stageIndex];
            auto &reflect = shader->reflection;
            reflect.EnumeratePushConstantBlocks(&count, nullptr);

            if (count) {
                Assert(count == 1, "shader cannot have multiple push constant blocks");
                auto pushConstant = reflect.GetPushConstantBlock(0);

                pushConstantRange.offset = pushConstant->offset;
                pushConstantRange.size = std::max(pushConstantRange.size, pushConstant->size);
                Assert(pushConstantRange.size <= MAX_PUSH_CONSTANT_SIZE, "push constant size overflow");
                pushConstantRange.stageFlags |= stage;
            }
        }

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayoutInfo.pushConstantRangeCount = pushConstantRange.stageFlags ? 1 : 0;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        if (descriptorSetLayout) {
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        }
        layout = device->createPipelineLayoutUnique(pipelineLayoutInfo);
    }

    shared_ptr<Pipeline> PipelineManager::GetGraphicsPipeline(const PipelineCompileInput &compile) {
        ShaderSet shaders = FetchShaders(device, compile.state.shaders);

        PipelineKey key;
        key.input.state = compile.state;
        key.input.renderPass = compile.renderPass;
        key.input.shaderHashes = GetShaderHashes(shaders);

        if (!key.input.state.blendEnable) {
            key.input.state.blendOp = vk::BlendOp::eAdd;
            key.input.state.dstBlendFactor = vk::BlendFactor::eZero;
            key.input.state.srcBlendFactor = vk::BlendFactor::eZero;
        }

        auto &pipelineMapValue = pipelines[key];
        if (!pipelineMapValue) {
            auto layout = GetPipelineLayout(shaders, compile.state.descriptorSetLayout);
            pipelineMapValue = make_shared<Pipeline>(device, shaders, compile, layout);
        }
        return pipelineMapValue;
    }

    Pipeline::Pipeline(DeviceContext &device,
                       const ShaderSet &shaders,
                       const PipelineCompileInput &compile,
                       shared_ptr<PipelineLayout> layout)
        : layout(layout) {

        auto &state = compile.state;

        std::array<vk::PipelineShaderStageCreateInfo, (size_t)ShaderStage::Count> shaderStages;
        size_t stageCount = 0;

        for (size_t i = 0; i < (size_t)ShaderStage::Count; i++) {
            auto &shader = shaders[i];
            if (!shader) continue;

            auto &stage = shaderStages[stageCount++];
            stage.module = shader->GetModule();
            stage.pName = "main";
            stage.stage = ShaderStageToFlagBits[i];
        }

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

        vk::PipelineViewportStateCreateInfo viewportState;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        vk::DynamicState states[] = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };

        vk::PipelineDynamicStateCreateInfo dynamicState;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = states;

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.polygonMode = vk::PolygonMode::eFill;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = state.cullMode;
        rasterizer.frontFace = vk::FrontFace::eCounterClockwise;

        // TODO: support multiple attachments
        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.blendEnable = state.blendEnable;
        if (state.blendEnable) {
            colorBlendAttachment.colorBlendOp = state.blendOp;
            colorBlendAttachment.alphaBlendOp = state.blendOp;
            colorBlendAttachment.srcColorBlendFactor = state.srcBlendFactor;
            colorBlendAttachment.srcAlphaBlendFactor = state.srcBlendFactor;
            colorBlendAttachment.dstColorBlendFactor = state.dstBlendFactor;
            colorBlendAttachment.dstAlphaBlendFactor = state.dstBlendFactor;
        }
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.sampleShadingEnable = false;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineDepthStencilStateCreateInfo depthStencil;
        depthStencil.depthTestEnable = state.depthTest;
        depthStencil.depthWriteEnable = state.depthWrite;
        depthStencil.depthCompareOp = vk::CompareOp::eLess;
        depthStencil.depthBoundsTestEnable = false;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = state.stencilTest;

        vk::PipelineVertexInputStateCreateInfo VertexLayout;
        VertexLayout.vertexBindingDescriptionCount = state.vertexLayout.bindingCount;
        VertexLayout.pVertexBindingDescriptions = state.vertexLayout.bindings.data();
        VertexLayout.vertexAttributeDescriptionCount = state.vertexLayout.attributeCount;
        VertexLayout.pVertexAttributeDescriptions = state.vertexLayout.attributes.data();

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.stageCount = stageCount;
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &VertexLayout;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = **layout;
        pipelineInfo.renderPass = compile.renderPass;
        pipelineInfo.subpass = 0;

        auto pipelinesResult = device->createGraphicsPipelinesUnique({}, {pipelineInfo});
        AssertVKSuccess(pipelinesResult.result, "creating pipelines");
        pipeline = std::move(pipelinesResult.value[0]);
    }

} // namespace sp::vulkan
