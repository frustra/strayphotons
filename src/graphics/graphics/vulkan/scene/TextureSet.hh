/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/Async.hh"
#include "assets/Gltf.hh"
#include "core/DispatchQueue.hh"
#include "core/Hashing.hh"
#include "graphics/vulkan/core/Image.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"

namespace sp::vulkan {
    typedef uint16 TextureIndex;

    struct TextureHandle {
        TextureIndex index = 0;
        AsyncPtr<void> ref = nullptr;

        bool Ready() const {
            return !ref || ref->Ready();
        }
    };

    class TextureSet {
    public:
        TextureSet(DeviceContext &device, DispatchQueue &workQueue);

        TextureHandle LoadAssetImage(const string &name, bool genMipmap = false, bool srgb = true);
        TextureHandle LoadGltfMaterial(const shared_ptr<const Gltf> &source, int materialIndex, TextureType type);

        TextureHandle Add(const ImageCreateInfo &imageInfo,
            const ImageViewCreateInfo &viewInfo,
            const InitialData &data);
        TextureHandle Add(const ImageViewPtr &ptr);
        TextureHandle Add(const AsyncPtr<ImageView> &asyncPtr);

        ImageViewPtr Get(TextureIndex i) {
            Assertf(i < textures.size(), "Invalid texture index: %u", i);
            return textures[i];
        }

        ImageViewPtr GetSinglePixel(glm::vec4 value);
        TextureIndex GetSinglePixelIndex(glm::vec4 value);

        vk::DescriptorSet GetDescriptorSet() const {
            return textureDescriptorSet;
        }

        TextureIndex Count() const {
            return static_cast<TextureIndex>(textures.size());
        }

        void Flush();

    private:
        ImageViewPtr CreateSinglePixel(glm::u8vec4 value);
        void ReleaseTexture(TextureIndex i);
        TextureIndex AllocateTextureIndex();

        vector<ImageViewPtr> textures;
        vector<ImageViewPtr> texturesPendingDelete;

        vector<TextureIndex> freeTextureIndexes;
        vector<TextureIndex> texturesToFlush;
        vk::DescriptorSet textureDescriptorSet;

        robin_hood::unordered_map<string, TextureHandle> textureCache;
        robin_hood::unordered_map<uint32_t, TextureIndex> singlePixelMap;

        DeviceContext &device;
        DispatchQueue &workQueue;
    };
} // namespace sp::vulkan
