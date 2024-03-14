/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "graphics/vulkan/core/VertexLayout.hh"

namespace sp::vulkan {
    struct TextureVertex {
        glm::vec3 position;
        glm::vec2 uv;

        static VertexLayout Layout() {
            static VertexLayout info;
            if (!info.bindingCount) {
                info.PushBinding(0, sizeof(TextureVertex));
                info.PushAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(TextureVertex, position));
                info.PushAttribute(2, 0, vk::Format::eR32G32Sfloat, offsetof(TextureVertex, uv));
            }
            return info;
        }
    };

    struct SceneVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;

        static VertexLayout Layout() {
            static VertexLayout info;
            if (!info.bindingCount) {
                info.PushBinding(0, sizeof(SceneVertex));
                info.PushAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, position));
                info.PushAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(SceneVertex, normal));
                info.PushAttribute(2, 0, vk::Format::eR32G32Sfloat, offsetof(SceneVertex, uv));
            }
            return info;
        }
    };

    struct ColorVertex2D {
        glm::vec2 position;
        glm::vec3 color;

        static VertexLayout Layout() {
            static VertexLayout info;
            if (!info.bindingCount) {
                info.PushBinding(0, sizeof(ColorVertex2D));
                info.PushAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(ColorVertex2D, position));
                info.PushAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(ColorVertex2D, color));
            }
            return info;
        }
    };

    struct PositionVertex2D {
        glm::vec2 position;

        static VertexLayout Layout() {
            static VertexLayout info;
            if (!info.bindingCount) {
                info.PushBinding(0, sizeof(PositionVertex2D));
                info.PushAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(PositionVertex2D, position));
            }
            return info;
        }
    };

    struct JointVertex {
        glm::vec4 jointWeights;
        glm::u16vec4 jointIndexes;
        float _padding[2];

        static void AddLayout(VertexLayout &layout, int binding) {
            layout.PushBinding(binding, sizeof(JointVertex));
            layout.PushAttribute(0, 0, vk::Format::eR16G16B16A16Uint, offsetof(JointVertex, jointIndexes));
            layout.PushAttribute(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(JointVertex, jointWeights));
        }

        static VertexLayout Layout() {
            static VertexLayout info;
            if (!info.bindingCount) AddLayout(info, 0);
            return info;
        }
    };

} // namespace sp::vulkan
