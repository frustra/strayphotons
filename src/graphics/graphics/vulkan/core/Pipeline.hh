/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Hashing.hh"
#include "graphics/vulkan/core/Shader.hh"
#include "graphics/vulkan/core/VertexLayout.hh"
#include "graphics/vulkan/core/VkCommon.hh"

#include <bitset>
#include <robin_hood.h>
#include <spirv_reflect.h>

namespace sp::vulkan {
    class Model;
    class RenderPass;

    struct SpecializationData {
        std::array<uint32, MAX_SPEC_CONSTANTS> values = {};
        std::bitset<MAX_SPEC_CONSTANTS> set = {};
    };

    struct PipelineStaticState {
        ShaderHandleSet shaders = {};
        VertexLayout vertexLayout;
        vk::PrimitiveTopology primitiveTopology = vk::PrimitiveTopology::eTriangleList;
        vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
        vk::CullModeFlags cullMode;
        vk::FrontFace frontFaceWinding;
        float lineWidth = 1.0f;
        vk::BlendOp blendOp;
        vk::BlendFactor srcBlendFactor, srcAlphaBlendFactor;
        vk::BlendFactor dstBlendFactor, dstAlphaBlendFactor;
        unsigned depthWrite : 1;
        unsigned depthTest : 1;
        unsigned blendEnable : 1;
        unsigned stencilTest : 1;
        uint8 viewportCount = 1;
        uint8 scissorCount = 1;
        vk::CompareOp depthCompareOp = vk::CompareOp::eLess;
        vk::CompareOp stencilCompareOp = vk::CompareOp::eAlways;
        vk::StencilOp stencilFailOp = vk::StencilOp::eKeep;
        vk::StencilOp stencilDepthFailOp = vk::StencilOp::eKeep;
        vk::StencilOp stencilPassOp = vk::StencilOp::eKeep;
        sp::EnumArray<SpecializationData, ShaderStage> specializations = {};
    };

    struct PipelineCompileInput {
        PipelineStaticState state;
        shared_ptr<RenderPass> renderPass;
    };

    struct DescriptorSetLayoutInfo {
        uint32 sampledImagesMask = 0;
        uint32 uniformBuffersMask = 0;
        uint32 storageBuffersMask = 0;
        uint32 storageImagesMask = 0;
        uint32 lastBinding = 0;

        vk::ShaderStageFlags stages[MAX_BINDINGS_PER_DESCRIPTOR_SET] = {};

        // count is usually 1, can be higher for array bindings, or 0 for an unbounded array
        uint8 descriptorCount[MAX_BINDINGS_PER_DESCRIPTOR_SET] = {};
    };

    struct PipelineLayoutInfo {
        vk::PushConstantRange pushConstantRange;

        uint32 descriptorSetsMask = 0;
        uint32 bindlessMask = 0;
        DescriptorSetLayoutInfo descriptorSets[MAX_BOUND_DESCRIPTOR_SETS] = {};

        struct MemorySize {
            vk::DeviceSize sizeBase = 0, sizeIncrement = 0;
        };
        std::array<std::array<MemorySize, MAX_BINDINGS_PER_DESCRIPTOR_SET>, MAX_BOUND_DESCRIPTOR_SETS> sizes;
    };

    // meta-pool that creates multiple descriptor pools as needed
    class DescriptorPool {
    public:
        DescriptorPool(DeviceContext &device, const DescriptorSetLayoutInfo &layoutInfo);

        std::pair<vk::DescriptorSet, bool> GetDescriptorSet(Hash64 hash);

        vk::DescriptorSet CreateBindlessDescriptorSet();

        vk::DescriptorSetLayout GetDescriptorSetLayout() const {
            return *descriptorSetLayout;
        }

    private:
        DeviceContext &device;

        vector<vk::DescriptorSetLayoutBinding> bindings;
        vector<vk::DescriptorPoolSize> sizes;
        vk::UniqueDescriptorSetLayout descriptorSetLayout;

        robin_hood::unordered_map<Hash64, vk::DescriptorSet> filledSets;
        vector<vk::DescriptorSet> freeSets;
        vector<vk::UniqueDescriptorPool> usedPools;

        bool bindless = false;
    };

    class PipelineManager;

    class PipelineLayout : public WrappedUniqueHandle<vk::PipelineLayout> {
    public:
        PipelineLayout(DeviceContext &device, const ShaderSet &shaders, PipelineManager &manager);

        const PipelineLayoutInfo &Info() const {
            return info;
        }

        vk::DescriptorUpdateTemplate GetDescriptorUpdateTemplate(uint32 set) const {
            if (HasDescriptorSet(set)) {
                return *descriptorUpdateTemplates[set];
            }
            return VK_NULL_HANDLE;
        }

        bool HasDescriptorSet(uint32 set) const {
            return info.descriptorSetsMask & (1 << set);
        }

        bool IsBindlessSet(uint32 set) const {
            return info.bindlessMask & (1 << set);
        }

        vk::DescriptorSet GetFilledDescriptorSet(uint32 set, const DescriptorSetBindings &setBindings);

    private:
        void ReflectShaders();
        void CreateDescriptorUpdateTemplates(DeviceContext &device);

        DeviceContext &device;
        ShaderSet shaders;
        PipelineLayoutInfo info;
        vk::UniqueDescriptorUpdateTemplate descriptorUpdateTemplates[MAX_BOUND_DESCRIPTOR_SETS];
        shared_ptr<DescriptorPool> descriptorPools[MAX_BOUND_DESCRIPTOR_SETS];
    };

    class Pipeline : public WrappedUniqueHandle<vk::Pipeline> {
    public:
        Pipeline(DeviceContext &device,
            const ShaderSet &shaders,
            const PipelineCompileInput &compile,
            shared_ptr<PipelineLayout> layout);

        shared_ptr<PipelineLayout> GetLayout() const {
            return layout;
        }

    private:
        shared_ptr<PipelineLayout> layout;
    };

    class PipelineManager : public NonCopyable {
    public:
        PipelineManager(DeviceContext &device);

        shared_ptr<Pipeline> GetPipeline(const PipelineCompileInput &compile);
        shared_ptr<PipelineLayout> GetPipelineLayout(const ShaderSet &shaders);
        shared_ptr<DescriptorPool> GetDescriptorPool(const DescriptorSetLayoutInfo &layout);

        struct PipelineKeyData {
            ShaderHashSet shaderHashes;
            UniqueID renderPassID;
            PipelineStaticState state;
        };

        struct PipelineLayoutKeyData {
            ShaderHashSet shaderHashes;
        };

        using PipelineKey = HashKey<PipelineKeyData>;
        using PipelineLayoutKey = HashKey<PipelineLayoutKeyData>;
        using DescriptorPoolKey = HashKey<DescriptorSetLayoutInfo>;

    private:
        DeviceContext &device;
        vk::UniquePipelineCache pipelineCache;

        template<typename K, typename V>
        using mapType = robin_hood::unordered_flat_map<K, V, typename K::Hasher>;

        mapType<PipelineKey, shared_ptr<Pipeline>> pipelines;
        mapType<PipelineLayoutKey, shared_ptr<PipelineLayout>> pipelineLayouts;
        mapType<DescriptorPoolKey, shared_ptr<DescriptorPool>> descriptorPools;
    };
} // namespace sp::vulkan
