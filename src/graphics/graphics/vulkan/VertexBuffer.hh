#pragma once

#include "Common.hh"
#include "core/Common.hh"

#include <vector>

namespace sp::vulkan {
    const int MAX_VERTEX_ATTRIBUTES = 4;

    struct VertexInputInfo {
        VertexInputInfo(uint32 binding = 0,
                        uint32 stride = 0,
                        vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex)
            : VertexInputInfo(vk::VertexInputBindingDescription(binding, stride, inputRate)) {}

        VertexInputInfo(vk::VertexInputBindingDescription binding) : binding(binding) {}

        void PushAttribute(uint32 location, uint32 binding, vk::Format format, uint32 offset) {
            PushAttribute(vk::VertexInputAttributeDescription(location, binding, format, offset));
        }

        void PushAttribute(vk::VertexInputAttributeDescription attribute) {
            Assert(attributeCount < MAX_VERTEX_ATTRIBUTES, "too many vertex attributes");
            attributes[attributeCount++] = attribute;
        }

        const vk::PipelineVertexInputStateCreateInfo &PipelineInputInfo() {
            pipelineInputInfo.vertexBindingDescriptionCount = 1;
            pipelineInputInfo.pVertexBindingDescriptions = &binding;
            pipelineInputInfo.vertexAttributeDescriptionCount = attributeCount;
            pipelineInputInfo.pVertexAttributeDescriptions = attributes;
            return pipelineInputInfo;
        }

    private:
        vk::PipelineVertexInputStateCreateInfo pipelineInputInfo;
        vk::VertexInputBindingDescription binding;
        vk::VertexInputAttributeDescription attributes[MAX_VERTEX_ATTRIBUTES];
        size_t attributeCount = 0;
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
} // namespace sp::vulkan
