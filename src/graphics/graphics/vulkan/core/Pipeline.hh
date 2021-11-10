#pragma once

#include "core/Hashing.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Shader.hh"
#include "graphics/vulkan/core/Vertex.hh"

#include <SPIRV-Reflect/spirv_reflect.h>
#include <bitset>
#include <robin_hood.h>

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
        vk::CullModeFlags cullMode;
        vk::FrontFace frontFaceWinding;
        vk::BlendOp blendOp;
        vk::BlendFactor srcBlendFactor;
        vk::BlendFactor dstBlendFactor;
        unsigned depthWrite : 1;
        unsigned depthTest : 1;
        unsigned blendEnable : 1;
        unsigned stencilTest : 1;
        std::array<SpecializationData, (size_t)ShaderStage::Count> specializations = {};
    };

    struct PipelineCompileInput {
        PipelineStaticState state;
        shared_ptr<RenderPass> renderPass;
    };

    struct DescriptorSetLayoutInfo {
        uint32 sampledImagesMask = 0;
        uint32 uniformBuffersMask = 0;
        uint32 storageImagesMask = 0;
        uint8 descriptorCount[MAX_BINDINGS_PER_DESCRIPTOR_SET]; // usually 1, can be higher for array bindings
        vk::ShaderStageFlags stages[MAX_BINDINGS_PER_DESCRIPTOR_SET];
        uint32 lastBinding = 0;
    };

    struct PipelineLayoutInfo {
        vk::PushConstantRange pushConstantRange;

        uint32 descriptorSetsMask = 0;
        DescriptorSetLayoutInfo descriptorSets[MAX_BOUND_DESCRIPTOR_SETS];
    };

    // meta-pool that creates multiple descriptor pools as needed
    class DescriptorPool {
    public:
        DescriptorPool(DeviceContext &device, const DescriptorSetLayoutInfo &layoutInfo);

        std::pair<vk::DescriptorSet, bool> GetDescriptorSet(Hash64 hash);

        vk::DescriptorSetLayout GetDescriptorSetLayout() const {
            return *descriptorSetLayout;
        }

    private:
        DeviceContext &device;

        vector<vk::DescriptorPoolSize> sizes;
        vk::UniqueDescriptorSetLayout descriptorSetLayout;

        robin_hood::unordered_map<Hash64, vk::DescriptorSet> filledSets;
        vector<vk::DescriptorSet> freeSets;
        vector<vk::UniqueDescriptorPool> usedPools;
    };

    class PipelineManager;

    class PipelineLayout : public WrappedUniqueHandle<vk::PipelineLayout> {
    public:
        PipelineLayout(DeviceContext &device, const ShaderSet &shaders, PipelineManager &manager);

        const PipelineLayoutInfo &Info() const {
            return info;
        }

        vk::DescriptorUpdateTemplate GetDescriptorUpdateTemplate(uint32 set) const {
            if (HasDescriptorSet(set)) { return *descriptorUpdateTemplates[set]; }
            return VK_NULL_HANDLE;
        }

        bool HasDescriptorSet(uint32 set) const {
            return info.descriptorSetsMask & (1 << set);
        }

        vk::DescriptorSet GetFilledDescriptorSet(uint32 set, const DescriptorSetBindings &setBindings);

    private:
        void ReflectShaders(const ShaderSet &shaders);
        void CreateDescriptorUpdateTemplates(DeviceContext &device);

        DeviceContext &device;
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
