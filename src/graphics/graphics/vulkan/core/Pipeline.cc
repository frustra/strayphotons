#include "Pipeline.hh"

#include "core/Logging.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Vertex.hh"

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

    shared_ptr<PipelineLayout> PipelineManager::GetPipelineLayout(const ShaderSet &shaders) {
        PipelineLayoutKey key;
        key.input.shaderHashes = GetShaderHashes(shaders);

        auto &mapValue = pipelineLayouts[key];
        if (!mapValue) { mapValue = make_shared<PipelineLayout>(device, shaders, *this); }
        return mapValue;
    }

    PipelineLayout::PipelineLayout(DeviceContext &device, const ShaderSet &shaders, PipelineManager &manager)
        : device(device) {
        ReflectShaders(shaders);

        vk::DescriptorSetLayout layouts[MAX_BOUND_DESCRIPTOR_SETS] = {};
        uint32 layoutCount = 0;

        for (uint32 set = 0; set < MAX_BOUND_DESCRIPTOR_SETS; set++) {
            if (!HasDescriptorSet(set)) continue;
            descriptorPools[set] = manager.GetDescriptorPool(info.descriptorSets[set]);
            layouts[set] = descriptorPools[set]->GetDescriptorSetLayout();
            layoutCount = set + 1;
        }

        // TODO: implement other fields on vk::PipelineLayoutCreateInfo
        // They should all be possible to determine with shader reflection.

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayoutInfo.pushConstantRangeCount = info.pushConstantRange.stageFlags ? 1 : 0;
        pipelineLayoutInfo.pPushConstantRanges = &info.pushConstantRange;
        pipelineLayoutInfo.setLayoutCount = layoutCount;
        pipelineLayoutInfo.pSetLayouts = layouts;
        uniqueHandle = device->createPipelineLayoutUnique(pipelineLayoutInfo);

        CreateDescriptorUpdateTemplates(device);
    }

    void PipelineLayout::ReflectShaders(const ShaderSet &shaders) {
        uint32 count;

        for (size_t stageIndex = 0; stageIndex < (size_t)ShaderStage::Count; stageIndex++) {
            auto &shader = shaders[stageIndex];
            if (!shader) continue;

            auto stage = ShaderStageToFlagBits[stageIndex];
            auto &reflect = shader->reflection;
            reflect.EnumeratePushConstantBlocks(&count, nullptr);

            if (count) {
                Assert(count == 1, "shader cannot have multiple push constant blocks");
                auto pushConstant = reflect.GetPushConstantBlock(0);

                info.pushConstantRange.offset = pushConstant->offset;
                info.pushConstantRange.size = std::max(info.pushConstantRange.size, pushConstant->size);
                Assert(info.pushConstantRange.size <= MAX_PUSH_CONSTANT_SIZE, "push constant size overflow");
                info.pushConstantRange.stageFlags |= stage;
            }

            reflect.EnumerateDescriptorSets(&count, nullptr);
            for (uint32 setIndex = 0; setIndex < count; setIndex++) {
                auto descriptorSet = reflect.GetDescriptorSet(setIndex);
                auto set = descriptorSet->set;
                Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "too many descriptor sets");

                auto &setInfo = info.descriptorSets[set];
                for (uint32 bindingIndex = 0; bindingIndex < descriptorSet->binding_count; bindingIndex++) {
                    auto &desc = descriptorSet->bindings[bindingIndex];
                    if (!desc->accessed) continue;

                    auto binding = desc->binding;
                    auto type = desc->descriptor_type;
                    Assert(binding < MAX_BINDINGS_PER_DESCRIPTOR_SET, "too many descriptors");

                    info.descriptorSetsMask |= (1 << set); // shader uses this descriptor set
                    setInfo.stages[binding] |= stage;
                    setInfo.lastBinding = std::max(setInfo.lastBinding, binding);

                    if (desc->array.dims_count) {
                        Assert(desc->array.dims_count == 1,
                               "only zero or one dimensional arrays of bindings are supported");
                        setInfo.descriptorCount[binding] = desc->array.dims[0];
                    } else {
                        setInfo.descriptorCount[binding] = 1;
                    }

                    if (type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                        Assert(desc->image.dim != SpvDimBuffer, "sampled buffers are unimplemented");
                        setInfo.sampledImagesMask |= (1 << binding);
                    } else if (type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
                        setInfo.storageImagesMask |= (1 << binding);
                    } else if (type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                        setInfo.uniformBuffersMask |= (1 << binding);
                    } else {
                        Abort("unsupported SpvReflectDescriptorType " + std::to_string(type));
                    }
                }
            }
        }
    }

    void PipelineLayout::CreateDescriptorUpdateTemplates(DeviceContext &device) {
        for (uint32 set = 0; set < MAX_BOUND_DESCRIPTOR_SETS; set++) {
            if (!HasDescriptorSet(set)) continue;

            vk::DescriptorUpdateTemplateEntry entries[MAX_BINDINGS_PER_DESCRIPTOR_SET];
            uint32 entryCount = 0;

            auto &setInfo = info.descriptorSets[set];
            auto setEntry = [&](uint32 binding, vk::DescriptorType type, size_t structOffset) {
                Assert(entryCount < MAX_BINDINGS_PER_DESCRIPTOR_SET, "too many descriptors");
                auto &entry = entries[entryCount++];
                entry.descriptorType = type;
                entry.dstBinding = binding;
                entry.dstArrayElement = 0;
                entry.descriptorCount = setInfo.descriptorCount[binding];

                entry.offset = sizeof(DescriptorBinding) * binding + structOffset;
                entry.stride = sizeof(DescriptorBinding);
            };

            ForEachBit(setInfo.sampledImagesMask, [&](uint32 binding) {
                setEntry(binding, vk::DescriptorType::eCombinedImageSampler, offsetof(DescriptorBinding, image));
            });
            ForEachBit(setInfo.storageImagesMask, [&](uint32 binding) {
                setEntry(binding, vk::DescriptorType::eStorageImage, offsetof(DescriptorBinding, image));
            });
            ForEachBit(setInfo.uniformBuffersMask, [&](uint32 binding) {
                setEntry(binding, vk::DescriptorType::eUniformBuffer, offsetof(DescriptorBinding, buffer));
            });

            vk::DescriptorUpdateTemplateCreateInfo createInfo;
            createInfo.set = set;
            createInfo.pipelineLayout = *uniqueHandle;
            createInfo.descriptorSetLayout = descriptorPools[set]->GetDescriptorSetLayout();
            createInfo.templateType = vk::DescriptorUpdateTemplateType::eDescriptorSet;
            createInfo.descriptorUpdateEntryCount = entryCount;
            createInfo.pDescriptorUpdateEntries = entries;
            createInfo.pipelineBindPoint = vk::PipelineBindPoint::eGraphics; // TODO: support compute

            descriptorUpdateTemplates[set] = device->createDescriptorUpdateTemplateUnique(createInfo);
        }
    }

    vk::DescriptorSet PipelineLayout::GetFilledDescriptorSet(uint32 set, const DescriptorSetBindings &setBindings) {
        if (!HasDescriptorSet(set)) return VK_NULL_HANDLE;

        auto &setLayout = info.descriptorSets[set];
        auto &bindings = setBindings.bindings;

        // Hash of the subset of data that is actually accessed by the pipeline.
        Hash64 hash = 0;

        ForEachBit(setLayout.sampledImagesMask | setLayout.storageImagesMask, [&](uint32 binding) {
            for (uint32 i = 0; i < setLayout.descriptorCount[binding]; i++) {
                // TODO: use UniqueID to avoid collisions upon pointer reuse
                hash_combine(hash, bindings[binding + i].image.imageView);
                // TODO: use UniqueID to avoid collisions upon pointer reuse
                hash_combine(hash, bindings[binding + i].image.sampler);
                hash_combine(hash, bindings[binding + i].image.imageLayout);
            }
        });

        ForEachBit(setLayout.uniformBuffersMask, [&](uint32 binding) {
            for (uint32 i = 0; i < setLayout.descriptorCount[binding]; i++) {
                // TODO: use UniqueID to avoid collisions upon pointer reuse
                hash_combine(hash, bindings[binding + i].buffer.buffer);
                hash_combine(hash, bindings[binding + i].buffer.offset);
                hash_combine(hash, bindings[binding + i].buffer.range);
            }
        });

        auto [descriptorSet, existed] = descriptorPools[set]->GetDescriptorSet(hash);
        if (!existed) {
            device->updateDescriptorSetWithTemplate(descriptorSet, GetDescriptorUpdateTemplate(set), &setBindings);
        }
        return descriptorSet;
    }

    shared_ptr<Pipeline> PipelineManager::GetPipeline(const PipelineCompileInput &compile) {
        ShaderSet shaders = FetchShaders(device, compile.state.shaders);

        PipelineKey key;
        key.input.state = compile.state;
        key.input.renderPass = compile.renderPass;
        key.input.shaderHashes = GetShaderHashes(shaders);

        for (size_t s = 0; s < (size_t)ShaderStage::Count; s++) {
            auto &specInInput = compile.state.specializations[s];
            auto &specInKey = key.input.state.specializations[s];
            for (size_t i = 0; i < MAX_SPEC_CONSTANTS; i++) {
                if (!specInInput.set[i]) Assert(!specInKey.values[i], "specialization provided but not set");
            }
        }

        if (!key.input.state.blendEnable) {
            key.input.state.blendOp = vk::BlendOp::eAdd;
            key.input.state.dstBlendFactor = vk::BlendFactor::eZero;
            key.input.state.srcBlendFactor = vk::BlendFactor::eZero;
        }

        auto &pipelineMapValue = pipelines[key];
        if (!pipelineMapValue) {
            auto layout = GetPipelineLayout(shaders);
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
        std::array<vk::SpecializationInfo, (size_t)ShaderStage::Count> shaderSpecialization;
        vector<vk::SpecializationMapEntry> specializationValues;
        size_t stageCount = 0, specCount = 0;

        for (size_t s = 0; s < (size_t)ShaderStage::Count; s++) {
            if (!shaders[s]) continue;
            auto &specIn = compile.state.specializations[s];
            if (specIn.set.any()) specCount += specIn.set.count();
        }

        specializationValues.reserve(specCount);
        for (size_t s = 0; s < (size_t)ShaderStage::Count; s++) {
            auto &shader = shaders[s];
            if (!shader) continue;

            auto &stage = shaderStages[stageCount++];
            stage.module = shader->GetModule();
            stage.pName = "main";
            stage.stage = ShaderStageToFlagBits[s];

            auto &specIn = compile.state.specializations[s];
            if (specIn.set.any()) {
                auto &specOut = shaderSpecialization[s];
                stage.pSpecializationInfo = &specOut;

                specOut.pData = specIn.values.data();
                specOut.dataSize = sizeof(specIn.values);
                specOut.mapEntryCount = 0;

                for (size_t i = 0; i < MAX_SPEC_CONSTANTS; i++) {
                    if (!specIn.set[i]) continue;
                    specializationValues.emplace_back(i, i * sizeof(uint32), sizeof(uint32));
                    specOut.mapEntryCount++;
                }

                specOut.pMapEntries = &specializationValues[specializationValues.size() - specOut.mapEntryCount];
            }
        }

        if (shaders[(size_t)ShaderStage::Compute]) {
            vk::ComputePipelineCreateInfo computeInfo;
            computeInfo.stage = shaderStages[0];
            computeInfo.layout = *layout;

            /* TODO: implement this if we need very fast shared memory
            https://www.khronos.org/blog/vulkan-subgroup-tutorial
            vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroupSizeInfo;
            computeInfo.stage.pNext = &subgroupSizeInfo;*/

            Assert(computeInfo.stage.stage == vk::ShaderStageFlagBits::eCompute, "multiple bound shaders");
            auto pipelinesResult = device->createComputePipelineUnique({}, computeInfo);
            AssertVKSuccess(pipelinesResult.result, "creating pipelines");
            uniqueHandle = std::move(pipelinesResult.value);
            return;
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
        rasterizer.frontFace = state.frontFaceWinding;

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
        pipelineInfo.layout = *layout;
        pipelineInfo.renderPass = compile.renderPass;
        pipelineInfo.subpass = 0;

        auto pipelinesResult = device->createGraphicsPipelineUnique({}, {pipelineInfo});
        AssertVKSuccess(pipelinesResult.result, "creating pipelines");
        uniqueHandle = std::move(pipelinesResult.value);
    }

    shared_ptr<DescriptorPool> PipelineManager::GetDescriptorPool(const DescriptorSetLayoutInfo &layout) {
        DescriptorPoolKey key(layout);
        auto &mapValue = descriptorPools[key];
        if (!mapValue) { mapValue = make_shared<DescriptorPool>(device, layout); }
        return mapValue;
    }

    DescriptorPool::DescriptorPool(DeviceContext &device, const DescriptorSetLayoutInfo &layoutInfo) : device(device) {
        vector<vk::DescriptorSetLayoutBinding> bindings;

        for (uint32 binding = 0; binding < layoutInfo.lastBinding + 1; binding++) {
            auto bindingBit = 1 << binding;
            vk::DescriptorType type;
            int count = 0;

            if (layoutInfo.sampledImagesMask & bindingBit) {
                type = vk::DescriptorType::eCombinedImageSampler;
                count++;
            }
            if (layoutInfo.storageImagesMask & bindingBit) {
                type = vk::DescriptorType::eStorageImage;
                count++;
            }
            if (layoutInfo.uniformBuffersMask & bindingBit) {
                type = vk::DescriptorType::eUniformBuffer;
                count++;
            }

            if (count > 0) {
                Assert(count == 1, "Overlapping descriptor binding index: " + std::to_string(binding));
                bindings.emplace_back(binding,
                                      type,
                                      layoutInfo.descriptorCount[binding],
                                      layoutInfo.stages[binding],
                                      nullptr);

                sizes.emplace_back(type, layoutInfo.descriptorCount[binding] * MAX_DESCRIPTOR_SETS_PER_POOL);
            }
        }

        vk::DescriptorSetLayoutCreateInfo createInfo;
        createInfo.bindingCount = bindings.size();
        createInfo.pBindings = bindings.data();
        descriptorSetLayout = device->createDescriptorSetLayoutUnique(createInfo);
    }

    std::pair<vk::DescriptorSet, bool> DescriptorPool::GetDescriptorSet(Hash64 hash) {
        auto &set = filledSets[hash];
        if (set) return {set, true};

        if (!freeSets.empty()) {
            set = freeSets.back();
            freeSets.pop_back();
            return {set, false};
        }

        vk::DescriptorPoolCreateInfo createInfo;
        createInfo.poolSizeCount = sizes.size();
        createInfo.pPoolSizes = sizes.data();
        createInfo.maxSets = MAX_DESCRIPTOR_SETS_PER_POOL;
        usedPools.push_back(device->createDescriptorPoolUnique(createInfo));

        vk::DescriptorSetLayout layouts[MAX_DESCRIPTOR_SETS_PER_POOL];
        std::fill(layouts, std::end(layouts), *descriptorSetLayout);

        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = *usedPools.back();
        allocInfo.descriptorSetCount = createInfo.maxSets;
        allocInfo.pSetLayouts = layouts;

        auto sets = device->allocateDescriptorSets(allocInfo);
        set = sets.back();
        sets.pop_back();
        freeSets.insert(freeSets.end(), sets.begin(), sets.end());
        return {set, false};
    }
} // namespace sp::vulkan
