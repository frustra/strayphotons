/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/EnumTypes.hh"
#include "common/Hashing.hh"
#include "common/InlineVector.hh"
#include "graphics/vulkan/core/VkCommon.hh"

#include <spirv_reflect.h>

namespace sp::vulkan {
    const uint32_t MAX_PUSH_CONSTANT_SIZE = 128;
    const uint32_t MAX_PUSH_CONSTANT_BLOCKS = 1;
    const uint32_t MAX_SPEC_CONSTANTS = 16;
    const uint32_t MAX_BOUND_DESCRIPTOR_SETS = 4;
    const uint32_t MAX_BINDINGS_PER_DESCRIPTOR_SET = 32;
    const uint32_t MAX_DESCRIPTOR_SETS_PER_POOL = 16;
    const uint32_t MAX_BINDINGS_PER_BINDLESS_DESCRIPTOR_SET = 640;

    class Model;

    enum class ShaderStage {
        Vertex,
        Geometry,
        Fragment,
        Compute,
    };

    static const sp::EnumArray<vk::ShaderStageFlagBits, ShaderStage> ShaderStageToFlagBits = {
        vk::ShaderStageFlagBits::eVertex,
        vk::ShaderStageFlagBits::eGeometry,
        vk::ShaderStageFlagBits::eFragment,
        vk::ShaderStageFlagBits::eCompute,
    };

    class Shader : public NonCopyable {
    public:
        Shader(const string &name,
            vk::UniqueShaderModule &&module,
            spv_reflect::ShaderModule &&reflection,
            Hash64 hash);

        const string name;
        const Hash64 hash; // SPIR-V buffer hash

        vk::ShaderModule GetModule() {
            return *shaderModule;
        }

        struct DescriptorSetBinding {
            std::string name;
            vk::DescriptorType type;
            uint32_t bindingId;
            bool accessed;
        };

        struct DescriptorSet {
            uint32_t setId;
            InlineVector<DescriptorSetBinding, MAX_BINDINGS_PER_DESCRIPTOR_SET> bindings;
        };

        struct SpecConstant {
            std::string name;
            uint32_t constantId;
        };

        struct PushConstant {
            uint32_t offset;
            uint32_t size;
        };

        const InlineVector<DescriptorSet, MAX_BOUND_DESCRIPTOR_SETS> descriptorSets;
        const InlineVector<SpecConstant, MAX_SPEC_CONSTANTS> specConstants;
        const InlineVector<PushConstant, MAX_PUSH_CONSTANT_BLOCKS> pushConstants;

    private:
        vk::UniqueShaderModule shaderModule;
        spv_reflect::ShaderModule reflection;

        friend class PipelineLayout;
    };

    // Sets that represent all of the shaders bound to one pipeline, indexed by stage
    typedef sp::EnumArray<shared_ptr<Shader>, ShaderStage> ShaderSet;
    typedef sp::EnumArray<ShaderHandle, ShaderStage> ShaderHandleSet;
    typedef sp::EnumArray<Hash64, ShaderStage> ShaderHashSet;

    struct DescriptorBinding {
        union {
            // Uses C API structs to allow for default initialization
            VkDescriptorBufferInfo buffer;
            VkBufferView buffer_view;
            VkDescriptorImageInfo image;
        };
        UniqueID uniqueID = 0;
        uint32_t arrayStride = 0;
    };

    struct DescriptorSetBindings {
        DescriptorBinding bindings[MAX_BINDINGS_PER_DESCRIPTOR_SET];
    };

    struct ShaderDataBindings {
        uint8_t pushConstants[MAX_PUSH_CONSTANT_SIZE];
        DescriptorSetBindings sets[MAX_BOUND_DESCRIPTOR_SETS];
    };
} // namespace sp::vulkan
