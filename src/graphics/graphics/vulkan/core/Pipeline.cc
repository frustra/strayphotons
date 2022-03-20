#include "Pipeline.hh"

#include "core/Logging.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

#include <SPIRV-Reflect/common/output_stream.h>

void StreamWriteDescriptorBinding(std::ostream &os,
    const SpvReflectDescriptorBinding &obj,
    bool write_set,
    bool flatten_cbuffers,
    const char *indent);

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
        : device(device), shaders(shaders) {
        ReflectShaders();

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

    void PipelineLayout::ReflectShaders() {
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
            Assert(count < MAX_BOUND_DESCRIPTOR_SETS, "too many descriptor sets");
            SpvReflectDescriptorSet *descriptorSets[MAX_BOUND_DESCRIPTOR_SETS];
            reflect.EnumerateDescriptorSets(&count, descriptorSets);

            for (uint32 setIndex = 0; setIndex < count; setIndex++) {
                auto descriptorSet = descriptorSets[setIndex];
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

                    if (desc->type_description->op == SpvOpTypeRuntimeArray || desc->array.dims_count) {
                        Assert(desc->array.dims_count == 1,
                            "only zero or one dimensional arrays of bindings are supported");
                        auto arraySize = desc->array.dims[0];
                        setInfo.descriptorCount[binding] = arraySize;
                        if (arraySize == 0) {
                            // descriptor is an unbounded array, like foo[]
                            info.bindlessMask |= (1 << set);
                        }
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

                        info.sizes[set][binding].sizeBase = desc->block.padded_size;
                    } else if (type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                        setInfo.storageBuffersMask |= (1 << binding);

                        vk::DeviceSize sizeBase = 0, sizeIncrement = 0;

                        if (desc->block.member_count > 0) {
                            auto &lastMember = desc->block.members[desc->block.member_count - 1];
                            sizeBase = lastMember.absolute_offset;

                            if (lastMember.type_description->op == SpvOpTypeRuntimeArray) {
                                Assert(lastMember.type_description->traits.array.stride > 0, "zero stride array");
                                // For runtime arrays, SPIRV-Reflect only sets the stride on the type description, not
                                // on the instance. Setting it on the instance here allows the stride to be printed in
                                // debug output.
                                lastMember.array.stride = lastMember.type_description->traits.array.stride;
                                sizeIncrement = lastMember.array.stride;
                            } else {
                                sizeBase += lastMember.padded_size;
                            }
                        }

                        auto &sizes = info.sizes[set][binding];
                        sizes.sizeBase = sizeBase;
                        sizes.sizeIncrement = sizeIncrement;
                    } else {
                        Abortf("unsupported SpvReflectDescriptorType %d", type);
                    }
                }
            }
        }
    }

    void PipelineLayout::CreateDescriptorUpdateTemplates(DeviceContext &device) {
        for (uint32 set = 0; set < MAX_BOUND_DESCRIPTOR_SETS; set++) {
            if (!HasDescriptorSet(set)) continue;
            if (IsBindlessSet(set)) continue; // bindless sets have variable size and must be updated manually

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
            ForEachBit(setInfo.storageBuffersMask, [&](uint32 binding) {
                setEntry(binding, vk::DescriptorType::eStorageBuffer, offsetof(DescriptorBinding, buffer));
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
        Assertf(!IsBindlessSet(set), "can't automatically fill bindless descriptor set %d", set);

        auto &setLayout = info.descriptorSets[set];
        auto &bindings = setBindings.bindings;

        // Hash of the subset of data that is actually accessed by the pipeline.
        Hash64 hash = 0;

        ForEachBit(setLayout.sampledImagesMask | setLayout.storageImagesMask, [&](uint32 binding) {
            for (uint32 i = 0; i < setLayout.descriptorCount[binding]; i++) {
                hash_combine(hash, bindings[binding + i].uniqueID);
                hash_combine(hash, bindings[binding + i].image.imageView);
                hash_combine(hash, bindings[binding + i].image.sampler);
                hash_combine(hash, bindings[binding + i].image.imageLayout);
            }
        });

        ForEachBit(setLayout.uniformBuffersMask | setLayout.storageBuffersMask, [&](uint32 binding) {
            for (uint32 i = 0; i < setLayout.descriptorCount[binding]; i++) {
                hash_combine(hash, bindings[binding + i].uniqueID);
                hash_combine(hash, bindings[binding + i].buffer.buffer);
                hash_combine(hash, bindings[binding + i].buffer.offset);
                hash_combine(hash, bindings[binding + i].buffer.range);
            }
        });

        auto [descriptorSet, existed] = descriptorPools[set]->GetDescriptorSet(hash);
        if (!existed) {
            auto &sizes = info.sizes[set];
            bool errors = false;

            ForEachBit(setLayout.uniformBuffersMask | setLayout.storageBuffersMask, [&](uint32 binding) {
                auto size = bindings[binding].buffer.range - bindings[binding].buffer.offset;
                auto minSize = sizes[binding].sizeBase;
                if (size == minSize) return;

                auto bindingArrayStride = sizes[binding].sizeIncrement;
                auto bufferArrayStride = bindings[binding].arrayStride;

                if (bufferArrayStride > 0 && bufferArrayStride == bindingArrayStride) {
                    int64_t delta = size - minSize;
                    if (delta > 0 && (delta % bufferArrayStride) == 0) return;
                }

                errors = true;

                spv_reflect::ShaderModule *reflect = nullptr;

                std::stringstream message;
                message << "Incompatible buffer layout in binding " << set << "." << binding << " accessed by shaders ";

                for (size_t i = 0; i < (size_t)ShaderStage::Count; i++) {
                    if (setLayout.stages[binding] & ShaderStageToFlagBits[i]) {
                        if (reflect) message << ", ";
                        message << shaders[i]->name;
                        reflect = &shaders[i]->reflection;
                    }
                }
                message << "\n";

                if (bufferArrayStride > 0 || bindingArrayStride > 0) {
                    message << "buffer (total size " << size << ", array stride " << bufferArrayStride << ")\n";
                    message << "binding (minimum size " << minSize << ", array stride " << bindingArrayStride << ")\n";
                } else {
                    message << "buffer (size " << size << ")\n";
                    message << "binding (size " << minSize << ")\n";
                }

                if (!reflect) {
                    message << "trying to write a descriptor value that's not accessed by any shader";
                } else {
                    auto desc = reflect->GetDescriptorBinding(binding, set);
                    StreamWriteDescriptorBinding(message, *desc, true, false, "  ");
                }

                Errorf("%s", message.str());
            });

            Assert(!errors, "error validating descriptor set");
            device->updateDescriptorSetWithTemplate(descriptorSet, GetDescriptorUpdateTemplate(set), &setBindings);
        }
        return descriptorSet;
    }

    shared_ptr<Pipeline> PipelineManager::GetPipeline(const PipelineCompileInput &compile) {
        ShaderSet shaders = FetchShaders(device, compile.state.shaders);

        PipelineKey key;
        key.input.state = compile.state;
        key.input.renderPassID = compile.renderPass ? compile.renderPass->GetUniqueID() : 0;
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
        inputAssembly.topology = state.primitiveTopology;

        vk::DynamicState states[] = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
            vk::DynamicState::eStencilWriteMask,
            vk::DynamicState::eStencilCompareMask,
            vk::DynamicState::eStencilReference,
        };

        vk::PipelineDynamicStateCreateInfo dynamicState;
        dynamicState.dynamicStateCount = 2;
        if (state.stencilTest) dynamicState.dynamicStateCount = 5;
        dynamicState.pDynamicStates = states;

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.polygonMode = vk::PolygonMode::eFill;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = !shaders[(size_t)ShaderStage::Fragment];
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = state.cullMode;
        rasterizer.frontFace = state.frontFaceWinding;

        std::array<vk::PipelineColorBlendAttachmentState, MAX_COLOR_ATTACHMENTS> colorBlendAttachments;

        vk::PipelineViewportStateCreateInfo viewportState;
        vk::PipelineColorBlendStateCreateInfo colorBlending;
        vk::PipelineMultisampleStateCreateInfo multisampling;
        vk::PipelineDepthStencilStateCreateInfo depthStencil;

        if (!rasterizer.rasterizerDiscardEnable) {
            viewportState.viewportCount = state.viewportCount;
            viewportState.scissorCount = state.scissorCount;

            colorBlending.attachmentCount = compile.renderPass->ColorAttachmentCount();
            colorBlending.pAttachments = colorBlendAttachments.data();

            // TODO: support configuring each attachment
            for (uint32 i = 0; i < colorBlending.attachmentCount; i++) {
                auto &blendState = colorBlendAttachments[i];
                blendState.blendEnable = state.blendEnable;
                if (state.blendEnable) {
                    blendState.colorBlendOp = state.blendOp;
                    blendState.alphaBlendOp = state.blendOp;
                    blendState.srcColorBlendFactor = state.srcBlendFactor;
                    blendState.srcAlphaBlendFactor = state.srcBlendFactor;
                    blendState.dstColorBlendFactor = state.dstBlendFactor;
                    blendState.dstAlphaBlendFactor = state.dstBlendFactor;
                }
                blendState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            }

            multisampling.sampleShadingEnable = false;
            multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

            depthStencil.depthTestEnable = state.depthTest;
            depthStencil.depthWriteEnable = state.depthWrite;
            depthStencil.depthCompareOp = state.depthCompareOp;
            depthStencil.depthBoundsTestEnable = false;
            depthStencil.minDepthBounds = 0.0f;
            depthStencil.maxDepthBounds = 1.0f;
            depthStencil.stencilTestEnable = state.stencilTest;
            depthStencil.front.compareOp = state.stencilCompareOp;
            depthStencil.front.failOp = state.stencilFailOp;
            depthStencil.front.depthFailOp = state.stencilDepthFailOp;
            depthStencil.front.passOp = state.stencilPassOp;
            depthStencil.back.compareOp = state.stencilCompareOp;
            depthStencil.back.failOp = state.stencilFailOp;
            depthStencil.back.depthFailOp = state.stencilDepthFailOp;
            depthStencil.back.passOp = state.stencilPassOp;
        }

        vk::PipelineVertexInputStateCreateInfo vertexLayout;
        vertexLayout.vertexBindingDescriptionCount = state.vertexLayout.bindingCount;
        vertexLayout.pVertexBindingDescriptions = state.vertexLayout.bindings.data();
        vertexLayout.vertexAttributeDescriptionCount = state.vertexLayout.attributeCount;
        vertexLayout.pVertexAttributeDescriptions = state.vertexLayout.attributes.data();

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.stageCount = stageCount;
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexLayout;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pRasterizationState = &rasterizer;
        if (!rasterizer.rasterizerDiscardEnable) {
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = &depthStencil;
            pipelineInfo.pColorBlendState = &colorBlending;
        }
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = *layout;
        pipelineInfo.renderPass = **compile.renderPass;
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
        vector<vk::DescriptorBindingFlags> bindingFlags;

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
            if (layoutInfo.storageBuffersMask & bindingBit) {
                type = vk::DescriptorType::eStorageBuffer;
                count++;
            }

            if (count > 0) {
                Assertf(count == 1, "Overlapping descriptor binding index: %u", binding);
                Assert(!bindless, "Variable length binding arrays must be the last binding in the set");

                auto stages = layoutInfo.stages[binding];
                uint32 descriptorCount = layoutInfo.descriptorCount[binding];
                if (descriptorCount == 0) {
                    bindless = true;
                    descriptorCount = MAX_BINDINGS_PER_BINDLESS_DESCRIPTOR_SET;
                    stages = vk::ShaderStageFlagBits::eAll;
                    bindingFlags.resize(binding + 1);
                    bindingFlags[binding] = vk::DescriptorBindingFlagBits::eVariableDescriptorCount |
                                            vk::DescriptorBindingFlagBits::ePartiallyBound |
                                            vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;
                }

                auto deviceLimit = device.Limits().maxDescriptorSetSampledImages;
                Assertf(descriptorCount <= deviceLimit,
                    "device supports %d sampler descriptors, wanted %d",
                    deviceLimit,
                    descriptorCount);

                bindings.emplace_back(binding, type, descriptorCount, stages, nullptr);
            }
        }

        vk::DescriptorSetLayoutCreateInfo createInfo;
        createInfo.bindingCount = bindings.size();
        createInfo.pBindings = bindings.data();

        vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo;
        if (!bindingFlags.empty()) {
            bindingFlagsCreateInfo.bindingCount = bindingFlags.size();
            bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();
            createInfo.pNext = &bindingFlagsCreateInfo;
            createInfo.flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
        }
        descriptorSetLayout = device->createDescriptorSetLayoutUnique(createInfo);
    }

    std::pair<vk::DescriptorSet, bool> DescriptorPool::GetDescriptorSet(Hash64 hash) {
        Assert(!bindless, "can't create a regular descriptor set for a bindless layout");

        auto &set = filledSets[hash];
        if (set) return {set, true};

        if (!freeSets.empty()) {
            set = freeSets.back();
            freeSets.pop_back();
            return {set, false};
        }

        if (sizes.empty()) {
            for (const auto &binding : bindings) {
                sizes.emplace_back(binding.descriptorType, binding.descriptorCount * MAX_DESCRIPTOR_SETS_PER_POOL);
            }
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

    vk::DescriptorSet DescriptorPool::CreateBindlessDescriptorSet() {
        Assert(bindless, "can't create a bindless descriptor set for a regular layout");

        // for now, just allocate the maximum
        // if we have multiple bindless sets, it would be good to tailor the sizes depending on the usage
        uint32 variableCount = bindings.back().descriptorCount;
        vk::DescriptorSetVariableDescriptorCountAllocateInfo countAllocInfo;
        countAllocInfo.descriptorSetCount = 1;
        countAllocInfo.pDescriptorCounts = &variableCount;

        if (sizes.empty()) {
            for (const auto &binding : bindings) {
                sizes.emplace_back(binding.descriptorType, binding.descriptorCount);
            }
        }

        vk::DescriptorPoolCreateInfo createInfo;
        createInfo.poolSizeCount = sizes.size();
        createInfo.pPoolSizes = sizes.data();
        createInfo.maxSets = 1;
        createInfo.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
        usedPools.push_back(device->createDescriptorPoolUnique(createInfo));

        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = *usedPools.back();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout.get();
        allocInfo.pNext = &countAllocInfo;

        auto sets = device->allocateDescriptorSets(allocInfo);
        return sets.front();
    }
} // namespace sp::vulkan
