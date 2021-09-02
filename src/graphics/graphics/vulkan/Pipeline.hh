#pragma once

#include "Common.hh"
#include "SPIRV-Reflect/spirv_reflect.h"
#include "Shader.hh"
#include "Vertex.hh"
#include "core/Hashing.hh"

#include <robin_hood.h>

namespace sp::vulkan {
    class DeviceContext;
    class Model;

    struct PipelineStaticState {
        ShaderHandleSet shaders;
        VertexLayout vertexLayout;
        vk::CullModeFlags cullMode;
        vk::BlendOp blendOp;
        vk::BlendFactor srcBlendFactor;
        vk::BlendFactor dstBlendFactor;
        unsigned depthWrite : 1;
        unsigned depthTest : 1;
        unsigned blendEnable : 1;
        unsigned stencilTest : 1;
    };

    struct PipelineCompileInput {
        PipelineCompileInput() {
            // Ensure the object is entirely zero for hashing, even if there are unaligned members.
            std::memset(&state, 0, sizeof(state));
        }

        PipelineStaticState state;
        vk::RenderPass renderPass; // TODO: use wrapper type to introspect attachments
    };

    class PipelineLayout {
    public:
        PipelineLayout(DeviceContext &device, const ShaderSet &shaders);

        vk::PipelineLayout operator*() const {
            return Get();
        }
        vk::PipelineLayout Get() const {
            return *layout;
        }

        vk::PushConstantRange pushConstantRange;

    private:
        vk::UniquePipelineLayout layout;
    };

    class Pipeline : public NonCopyable {
    public:
        Pipeline(DeviceContext &device,
                 const ShaderSet &shaders,
                 const PipelineCompileInput &compile,
                 shared_ptr<PipelineLayout> layout);

        vk::Pipeline operator*() const {
            return Get();
        }

        vk::Pipeline Get() const {
            return *pipeline;
        }

        shared_ptr<PipelineLayout> GetLayout() const {
            return layout;
        }

    private:
        shared_ptr<PipelineLayout> layout;
        vk::UniquePipeline pipeline;
    };

    class PipelineManager : public NonCopyable {
    public:
        PipelineManager(DeviceContext &device);

        shared_ptr<Pipeline> GetGraphicsPipeline(const PipelineCompileInput &compile);
        shared_ptr<PipelineLayout> GetPipelineLayout(const ShaderSet &shaders);

        struct PipelineKeyData {
            ShaderHashSet shaderHashes;
            VkRenderPass renderPass; // TODO: hash RenderPass important fields instead?
            PipelineStaticState state;
        };

        using PipelineKey = HashKey<PipelineKeyData>;
        using PipelineLayoutKey = HashKey<ShaderHashSet>;

    private:
        shared_ptr<Pipeline> CreateGraphicsPipeline(const ShaderSet &shaders, const PipelineCompileInput &compile);
        shared_ptr<PipelineLayout> CreatePipelineLayout(const ShaderSet &shaders);

        DeviceContext &device;
        vk::UniquePipelineCache pipelineCache;

        robin_hood::unordered_map<PipelineKey, shared_ptr<Pipeline>, PipelineKey::Hasher> pipelines;

        robin_hood::unordered_map<PipelineLayoutKey, shared_ptr<PipelineLayout>, PipelineLayoutKey::Hasher>
            pipelineLayouts;
    };
} // namespace sp::vulkan
