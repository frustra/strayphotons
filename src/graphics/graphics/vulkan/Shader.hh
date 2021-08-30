#pragma once

#include "Common.hh"
#include "SPIRV-Reflect/spirv_reflect.h"
#include "core/Hashing.hh"

namespace sp::vulkan {
    const size_t MAX_PUSH_CONSTANT_SIZE = 512;

    class DeviceContext;
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

    struct Shader : public NonCopyable {
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

    struct ShaderDataBindings {
        uint8 pushConstants[MAX_PUSH_CONSTANT_SIZE];
    };
} // namespace sp::vulkan
