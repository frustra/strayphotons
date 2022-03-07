#include "TextureSet.hh"

#include "assets/GltfImpl.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    TextureSet::TextureSet(DeviceContext &device, DispatchQueue &workQueue) : device(device), workQueue(workQueue) {
        textureDescriptorSet = device.CreateBindlessDescriptorSet();
        AllocateTextureIndex(); // reserve first index for blank pixel / error texture
    }

    TextureHandle TextureSet::Add(const ImageCreateInfo &imageInfo,
        const ImageViewCreateInfo &viewInfo,
        const InitialData &data) {
        auto i = AllocateTextureIndex();
        auto imageViewFut = device.CreateImageAndView(imageInfo, viewInfo, data);
        return {i, workQueue.Dispatch<void>(imageViewFut, [this, i](ImageViewPtr view) {
                    textures[i] = view;
                    texturesToFlush.push_back(i);
                })};
    }

    TextureHandle TextureSet::Add(const ImageViewPtr &ptr) {
        auto i = AllocateTextureIndex();
        textures[i] = ptr;
        texturesToFlush.push_back(i);
        return {i, {}};
    }

    TextureIndex TextureSet::AllocateTextureIndex() {
        TextureIndex i;
        if (!freeTextureIndexes.empty()) {
            i = freeTextureIndexes.back();
            freeTextureIndexes.pop_back();
        } else {
            i = textures.size();
            textures.emplace_back();
        }
        return i;
    }

    void TextureSet::ReleaseTexture(TextureIndex i) {
        textures[i].reset();
        freeTextureIndexes.push_back(i);
        texturesToFlush.push_back(i);
    }

    TextureHandle TextureSet::LoadGltfMaterial(const shared_ptr<const Gltf> &source,
        int materialIndex,
        TextureType type) {

        ZoneScoped;
        auto &gltfModel = *source->gltfModel;
        if (materialIndex < 0 || (size_t)materialIndex >= gltfModel.materials.size()) return {};
        auto &material = gltfModel.materials[materialIndex];

        string name = source->name + "_" + std::to_string(materialIndex) + "_";
        int textureIndex = -1;
        std::vector<double> factor;
        bool srgb = false;

        switch (type) {
        case TextureType::BaseColor:
            name += std::to_string(material.pbrMetallicRoughness.baseColorTexture.index) + "_BASE";
            textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            factor = material.pbrMetallicRoughness.baseColorFactor;
            srgb = true;
            break;

        // gltf2.0 uses a combined texture for metallic roughness.
        // Roughness = G channel, Metallic = B channel.
        // R and A channels are not used / should be ignored.
        // https://github.com/KhronosGroup/glTF/blob/e5519ce050/specification/2.0/schema/material.pbrMetallicRoughness.schema.json
        case TextureType::MetallicRoughness: {
            name += std::to_string(material.pbrMetallicRoughness.metallicRoughnessTexture.index) + "_METALLICROUGHNESS";
            textureIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            double rf = material.pbrMetallicRoughness.roughnessFactor,
                   mf = material.pbrMetallicRoughness.metallicFactor;
            if (rf != 1 || mf != 1) factor = {0.0, rf, mf, 0.0};
            // The spec says these should be linear, but we have srgb files right now.
            // Linear makes sense, there's no reason to have more precision for lower values.
            // TODO: reencode as linear
            srgb = true;
            break;
        }
        case TextureType::Height:
            name += std::to_string(material.normalTexture.index) + "_HEIGHT";
            textureIndex = material.normalTexture.index;
            // factor not supported for height textures
            break;

        case TextureType::Occlusion:
            name += std::to_string(material.occlusionTexture.index) + "_OCCLUSION";
            textureIndex = material.occlusionTexture.index;
            // factor not supported for occlusion textures
            break;

        case TextureType::Emissive:
            name += std::to_string(material.occlusionTexture.index) + "_EMISSIVE";
            textureIndex = material.emissiveTexture.index;
            factor = material.emissiveFactor;
            break;

        default:
            return {};
        }

        auto cacheEntry = textureCache.find(name);
        if (cacheEntry != textureCache.end()) return cacheEntry->second;

        if (textureIndex < 0 || (size_t)textureIndex >= gltfModel.textures.size()) {
            if (factor.size() == 0) factor.push_back(1); // default texture is a single white pixel

            auto data = make_shared<std::array<uint8, 4>>();
            for (size_t i = 0; i < 4; i++) {
                (*data)[i] = uint8(255.0 * factor.at(std::min(factor.size() - 1, i)));
            }

            // Create a single pixel texture based on the factor data provided
            ImageCreateInfo imageInfo;
            imageInfo.imageType = vk::ImageType::e2D;
            imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
            imageInfo.format = vk::Format::eR8G8B8A8Unorm;
            imageInfo.extent = vk::Extent3D(1, 1, 1);

            ImageViewCreateInfo viewInfo;
            viewInfo.defaultSampler = device.GetSampler(SamplerType::NearestTiled);
            auto pending = Add(imageInfo, viewInfo, {data->data(), data->size(), data});
            textureCache[name] = pending;
            return pending;
        }

        auto &texture = gltfModel.textures[textureIndex];
        if (texture.source < 0 || (size_t)texture.source >= gltfModel.images.size()) {
            Errorf("Gltf texture %d has invalid texture source: %d", textureIndex, texture.source);
            return {};
        }
        auto &img = gltfModel.images[texture.source];

        ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
        imageInfo.format = FormatFromTraits(img.component, img.bits, srgb);

        bool useFactor = false;
        for (auto f : factor) {
            if (f != 1) useFactor = true;
        }
        if (useFactor) imageInfo.factor = std::move(factor);

        if (imageInfo.format == vk::Format::eUndefined) {
            Errorf("Failed to load image at index %d: invalid format with components=%d and bits=%d",
                texture.source,
                img.component,
                img.bits);
            return {};
        }

        imageInfo.extent = vk::Extent3D(img.width, img.height, 1);

        ImageViewCreateInfo viewInfo;
        if (texture.sampler < 0 || (size_t)texture.sampler >= gltfModel.samplers.size()) {
            viewInfo.defaultSampler = device.GetSampler(SamplerType::TrilinearTiled);
            imageInfo.genMipmap = true;
        } else {
            auto &sampler = gltfModel.samplers[texture.sampler];
            int minFilter = sampler.minFilter > 0 ? sampler.minFilter : TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
            int magFilter = sampler.magFilter > 0 ? sampler.magFilter : TINYGLTF_TEXTURE_FILTER_LINEAR;

            auto samplerInfo = GLSamplerToVKSampler(minFilter, magFilter, sampler.wrapS, sampler.wrapT, sampler.wrapR);
            if (samplerInfo.mipmapMode == vk::SamplerMipmapMode::eLinear) {
                samplerInfo.anisotropyEnable = true;
                samplerInfo.maxAnisotropy = 8.0f;
            }
            viewInfo.defaultSampler = device.GetSampler(samplerInfo);
            imageInfo.genMipmap = (samplerInfo.maxLod > 0);
        }

        auto pending = Add(imageInfo, viewInfo, {img.image.data(), img.image.size(), source});
        textureCache[name] = pending;
        return pending;
    }

    void TextureSet::Flush() {
        texturesPendingDelete.clear();

        for (auto it = textureCache.begin(); it != textureCache.end();) {
            auto &handle = it->second;
            if (handle.ref.use_count() == 1) {
                // keep image alive until next frame to ensure no GPU references
                texturesPendingDelete.push_back(textures[handle.index]);
                ReleaseTexture(handle.index);
                it = textureCache.erase(it);
            } else {
                it++;
            }
        }

        vector<vk::WriteDescriptorSet> descriptorWrites;
        vector<vk::DescriptorImageInfo> descriptorImageInfos;
        vk::WriteDescriptorSet descriptorWrite;
        descriptorWrite.dstSet = textureDescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;

        for (auto descriptorIndex : texturesToFlush) {
            auto tex = textures[descriptorIndex];
            if (!tex) tex = GetBlankPixel();
            descriptorImageInfos.emplace_back(tex->DefaultSampler(), *tex, tex->Image()->LastLayout());
        }

        for (size_t queueIndex = 0; queueIndex < texturesToFlush.size();) {
            descriptorWrite.pImageInfo = &descriptorImageInfos[queueIndex];
            descriptorWrite.dstArrayElement = texturesToFlush[queueIndex];

            // compact consecutive descriptors into one write
            uint32 descriptorCount = 0;
            do {
                descriptorCount++;
                queueIndex++;
            } while (queueIndex < texturesToFlush.size() &&
                     texturesToFlush[queueIndex] == descriptorWrite.dstArrayElement + descriptorCount);

            descriptorWrite.descriptorCount = descriptorCount;
            descriptorWrites.push_back(descriptorWrite);
        }

        device->updateDescriptorSets(descriptorWrites, {});
        texturesToFlush.clear();
    }

    ImageViewPtr TextureSet::GetBlankPixel() {
        auto &texPtr = textures[0];
        if (!texPtr) texPtr = CreateSinglePixel(glm::vec4(1));
        return texPtr;
    }

    ImageViewPtr TextureSet::CreateSinglePixel(glm::vec4 value) {
        uint8 data[4];
        for (size_t i = 0; i < 4; i++) {
            data[i] = (uint8_t)(255.0 * value[i]);
        }

        ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
        imageInfo.format = vk::Format::eR8G8B8A8Unorm;
        imageInfo.extent = vk::Extent3D(1, 1, 1);

        ImageViewCreateInfo viewInfo;
        viewInfo.defaultSampler = device.GetSampler(SamplerType::NearestTiled);
        auto fut = device.CreateImageAndView(imageInfo, viewInfo, {data, sizeof(data)});
        device.FlushMainQueue();
        return fut->Get();
    }
} // namespace sp::vulkan
