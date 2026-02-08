/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "graphics/vulkan/core/VkCommon.hh"

namespace sp::vulkan {
    const int MAX_VERTEX_ATTRIBUTES = 5, MAX_VERTEX_INPUT_BINDINGS = 5;

    struct VertexLayout {
        VertexLayout() {}

        VertexLayout(uint32_t binding, uint32_t stride, vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex) {
            PushBinding(binding, stride, inputRate);
        }

        VertexLayout(uint32_t binding, size_t stride, vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex) {
            PushBinding(binding, (uint32_t)stride, inputRate);
        }

        void PushAttribute(uint32_t location, uint32_t binding, vk::Format format, uint32_t offset) {
            PushAttribute(vk::VertexInputAttributeDescription(location, binding, format, offset));
        }

        void PushAttribute(vk::VertexInputAttributeDescription attribute) {
            Assert(attributeCount < MAX_VERTEX_ATTRIBUTES, "too many vertex attributes");
            attributes[attributeCount++] = attribute;
        }

        void PushBinding(uint32_t binding,
            uint32_t stride,
            vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex) {
            PushBinding(vk::VertexInputBindingDescription(binding, stride, inputRate));
        }

        void PushBinding(vk::VertexInputBindingDescription binding) {
            Assert(bindingCount < MAX_VERTEX_INPUT_BINDINGS, "too many vertex input bindings");
            bindings[bindingCount++] = binding;
        }

        bool operator==(const VertexLayout &other) const {
            return std::memcmp(this, &other, sizeof(VertexLayout)) == 0;
        }

        bool operator!=(const VertexLayout &other) const {
            return !(*this == other);
        }

        std::array<vk::VertexInputBindingDescription, MAX_VERTEX_INPUT_BINDINGS> bindings;
        std::array<vk::VertexInputAttributeDescription, MAX_VERTEX_ATTRIBUTES> attributes;
        size_t bindingCount = 0, attributeCount = 0;
    };

} // namespace sp::vulkan
