/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Shader.hh"

#include "common/InlineVector.hh"

namespace sp::vulkan {
    InlineVector<Shader::DescriptorSet, MAX_BOUND_DESCRIPTOR_SETS> parseDescriptorSets(const string &name,
        spv_reflect::ShaderModule &reflection) {
        uint32_t count = 0;
        SpvReflectResult result = reflection.EnumerateDescriptorSets(&count, nullptr);
        Assertf(result == SPV_REFLECT_RESULT_SUCCESS,
            "reflection.EnumerateDescriptorSets(%s) returned: %s",
            name,
            result);

        std::vector<SpvReflectDescriptorSet *> reflectBuffer(count);
        result = reflection.EnumerateDescriptorSets(&count, reflectBuffer.data());
        Assertf(result == SPV_REFLECT_RESULT_SUCCESS,
            "reflection.EnumerateDescriptorSets(%s) returned: %s",
            name,
            result);

        InlineVector<Shader::DescriptorSet, MAX_BOUND_DESCRIPTOR_SETS> descriptorSets;
        for (auto *descriptorSet : reflectBuffer) {
            auto &set = descriptorSets.emplace_back(Shader::DescriptorSet{descriptorSet->set, {}});
            for (uint32 bindingIndex = 0; bindingIndex < descriptorSet->binding_count; bindingIndex++) {
                auto &binding = descriptorSet->bindings[bindingIndex];
                std::string bindingName = binding->name ? binding->name : "";
                if (bindingName.empty() && binding->type_description) {
                    bindingName = binding->type_description->type_name ? binding->type_description->type_name : "";
                }

                set.bindings.emplace_back(Shader::DescriptorSetBinding{
                    bindingName,
                    (vk::DescriptorType)binding->descriptor_type,
                    binding->binding,
                    !!binding->accessed,
                });
            }
        }
        return descriptorSets;
    }

    InlineVector<Shader::SpecConstant, MAX_SPEC_CONSTANTS> parseSpecConstants(const string &name,
        spv_reflect::ShaderModule &reflection) {
        uint32_t count = 0;
        SpvReflectResult result = reflection.EnumerateSpecializationConstants(&count, nullptr);
        Assertf(result == SPV_REFLECT_RESULT_SUCCESS,
            "reflection.EnumerateSpecializationConstants(%s) returned: %s",
            name,
            result);

        std::vector<SpvReflectSpecializationConstant *> reflectBuffer(count);
        result = reflection.EnumerateSpecializationConstants(&count, reflectBuffer.data());
        Assertf(result == SPV_REFLECT_RESULT_SUCCESS,
            "reflection.EnumerateSpecializationConstants(%s) returned: %s",
            name,
            result);

        InlineVector<Shader::SpecConstant, MAX_SPEC_CONSTANTS> specConstants;
        for (auto *specConstant : reflectBuffer) {
            specConstants.emplace_back(Shader::SpecConstant{
                specConstant->name ? specConstant->name : "",
                specConstant->constant_id,
            });
        }
        return specConstants;
    }

    InlineVector<Shader::PushConstant, MAX_PUSH_CONSTANT_BLOCKS> parsePushConstants(const string &name,
        spv_reflect::ShaderModule &reflection) {
        uint32_t count = 0;
        SpvReflectResult result = reflection.EnumeratePushConstantBlocks(&count, nullptr);
        Assertf(result == SPV_REFLECT_RESULT_SUCCESS,
            "reflection.EnumeratePushConstantBlocks(%s) returned: %s",
            name,
            result);

        std::vector<SpvReflectBlockVariable *> reflectBuffer(count);
        result = reflection.EnumeratePushConstantBlocks(&count, reflectBuffer.data());
        Assertf(result == SPV_REFLECT_RESULT_SUCCESS,
            "reflection.EnumeratePushConstantBlocks(%s) returned: %s",
            name,
            result);

        InlineVector<Shader::PushConstant, MAX_PUSH_CONSTANT_BLOCKS> pushConstants;
        for (auto *pushConstant : reflectBuffer) {
            pushConstants.emplace_back(Shader::PushConstant{
                pushConstant->offset,
                pushConstant->size,
            });
        }
        return pushConstants;
    }

    Shader::Shader(const string &name,
        vk::UniqueShaderModule &&module,
        spv_reflect::ShaderModule &&reflection,
        Hash64 hash)
        : name(name), hash(hash), descriptorSets(parseDescriptorSets(name, reflection)),
          specConstants(parseSpecConstants(name, reflection)), pushConstants(parsePushConstants(name, reflection)),
          shaderModule(std::move(module)), reflection(std::move(reflection)) {}
} // namespace sp::vulkan
