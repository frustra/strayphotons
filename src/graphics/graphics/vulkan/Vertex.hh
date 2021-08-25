#pragma once

#include "Common.hh"
#include "core/Common.hh"

#include <vector>

namespace sp::vulkan {
    const int MAX_VERTEX_ATTRIBUTES = 5, MAX_VERTEX_INPUT_BINDINGS = 5;

    struct VertexInputInfo {
        VertexInputInfo() {}

        VertexInputInfo(uint32 binding, uint32 stride, vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex) {
            PushBinding(binding, stride, inputRate);
        }

        void PushAttribute(uint32 location, uint32 binding, vk::Format format, uint32 offset) {
            PushAttribute(vk::VertexInputAttributeDescription(location, binding, format, offset));
        }

        void PushAttribute(vk::VertexInputAttributeDescription attribute) {
            Assert(attributeCount < MAX_VERTEX_ATTRIBUTES, "too many vertex attributes");
            attributes[attributeCount++] = attribute;
        }

        void PushBinding(uint32 binding, uint32 stride, vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex) {
            PushBinding(vk::VertexInputBindingDescription(binding, stride, inputRate));
        }

        void PushBinding(vk::VertexInputBindingDescription binding) {
            Assert(bindingCount < MAX_VERTEX_INPUT_BINDINGS, "too many vertex input bindings");
            bindings[bindingCount++] = binding;
        }

        const vk::PipelineVertexInputStateCreateInfo &PipelineInputInfo() {
            pipelineInputInfo.vertexBindingDescriptionCount = bindingCount;
            pipelineInputInfo.pVertexBindingDescriptions = bindings.data();
            pipelineInputInfo.vertexAttributeDescriptionCount = attributeCount;
            pipelineInputInfo.pVertexAttributeDescriptions = attributes.data();
            return pipelineInputInfo;
        }

    private:
        vk::PipelineVertexInputStateCreateInfo pipelineInputInfo;
        std::array<vk::VertexInputBindingDescription, MAX_VERTEX_INPUT_BINDINGS> bindings;
        std::array<vk::VertexInputAttributeDescription, MAX_VERTEX_ATTRIBUTES> attributes;
        size_t bindingCount = 0, attributeCount = 0;
    };

    struct TextureVertex {
        glm::vec3 position;
        glm::vec2 uv;

        static VertexInputInfo InputInfo() {
            VertexInputInfo info(0, sizeof(TextureVertex));
            info.PushAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(TextureVertex, position));
            info.PushAttribute(2, 0, vk::Format::eR32G32Sfloat, offsetof(TextureVertex, uv));
            return info;
        }
    };

    struct SceneVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;

        static VertexInputInfo InputInfo() {
            VertexInputInfo info(0, sizeof(SceneVertex));
            info.PushAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, position));
            info.PushAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, normal));
            info.PushAttribute(2, 0, vk::Format::eR32G32Sfloat, offsetof(SceneVertex, uv));
            return info;
        }
    };

    struct ColorVertex2D {
        glm::vec2 position;
        glm::vec3 color;

        static VertexInputInfo InputInfo() {
            VertexInputInfo info(0, sizeof(ColorVertex2D));
            info.PushAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(ColorVertex2D, position));
            info.PushAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(ColorVertex2D, color));
            return info;
        }
    };

} // namespace sp::vulkan
