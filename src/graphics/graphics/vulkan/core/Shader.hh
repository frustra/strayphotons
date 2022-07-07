#pragma once

#include "SPIRV-Reflect/spirv_reflect.h"
#include "core/Hashing.hh"
#include "graphics/vulkan/core/Common.hh"

namespace sp::vulkan {
    const uint32 MAX_PUSH_CONSTANT_SIZE = 128;
    const uint32 MAX_SPEC_CONSTANTS = 16;
    const uint32 MAX_BOUND_DESCRIPTOR_SETS = 4;
    const uint32 MAX_BINDINGS_PER_DESCRIPTOR_SET = 32;
    const uint32 MAX_DESCRIPTOR_SETS_PER_POOL = 16;
    const uint32 MAX_BINDINGS_PER_BINDLESS_DESCRIPTOR_SET = 65535;

    class Model;

    enum class ShaderStage {
        Vertex,
        Geometry,
        Fragment,
        Compute,
        Count,
    };

    static const std::array<vk::ShaderStageFlagBits, (size_t)ShaderStage::Count> ShaderStageToFlagBits = {
        vk::ShaderStageFlagBits::eVertex,
        vk::ShaderStageFlagBits::eGeometry,
        vk::ShaderStageFlagBits::eFragment,
        vk::ShaderStageFlagBits::eCompute,
    };

    class Shader : public NonCopyable {
    public:
        Shader(const string &name, vk::UniqueShaderModule &&module, spv_reflect::ShaderModule &&reflection, Hash64 hash)
            : name(name), hash(hash), reflection(std::move(reflection)), shaderModule(std::move(module)) {}

        const string name;
        const Hash64 hash; // SPIR-V buffer hash

        vk::ShaderModule GetModule() {
            return *shaderModule;
        }

        spv_reflect::ShaderModule reflection;

    private:
        vk::UniqueShaderModule shaderModule;
    };

    // Sets that represent all of the shaders bound to one pipeline, indexed by stage
    typedef std::array<shared_ptr<Shader>, (size_t)ShaderStage::Count> ShaderSet;
    typedef std::array<ShaderHandle, (size_t)ShaderStage::Count> ShaderHandleSet;
    typedef std::array<Hash64, (size_t)ShaderStage::Count> ShaderHashSet;

    struct DescriptorBinding {
        union {
            // Uses C API structs to allow for default initialization
            VkDescriptorBufferInfo buffer;
            VkBufferView buffer_view;
            VkDescriptorImageInfo image;
        };
        UniqueID uniqueID = 0;
        uint32 arrayStride = 0;
    };

    struct DescriptorSetBindings {
        DescriptorBinding bindings[MAX_BINDINGS_PER_DESCRIPTOR_SET];
    };

    struct ShaderDataBindings {
        uint8 pushConstants[MAX_PUSH_CONSTANT_SIZE];
        DescriptorSetBindings sets[MAX_BOUND_DESCRIPTOR_SETS];
    };
} // namespace sp::vulkan
