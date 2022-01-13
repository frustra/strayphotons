#include "graphics/vulkan/GPUSceneContext.hh"

#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    GPUSceneContext::GPUSceneContext(DeviceContext &device) : device(device) {
        textureDescriptorSet = device.CreateBindlessDescriptorSet();

        indexBuffer = device.AllocateBuffer(1024 * 1024 * 10,
            vk::BufferUsageFlagBits::eIndexBuffer,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        vertexBuffer = device.AllocateBuffer(1024 * 1024 * 100,
            vk::BufferUsageFlagBits::eVertexBuffer,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        primitiveLists = device.AllocateBuffer(1024 * 1024,
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        models = device.AllocateBuffer(1024 * 10, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);

        renderableEntityList = device.AllocateBuffer(1024 * 1024,
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    TextureIndex GPUSceneContext::AddTexture(const ImageViewPtr &ptr) {
        TextureIndex i;
        if (!freeTextureIndexes.empty()) {
            i = freeTextureIndexes.back();
            freeTextureIndexes.pop_back();
            textures[i] = ptr;
        } else {
            i = textures.size();
            textures.push_back(ptr);
        }
        texturesToFlush.push_back(i);
        return i;
    }

    void GPUSceneContext::ReleaseTexture(TextureIndex i) {
        textures[i].reset();
        freeTextureIndexes.push_back(i);
    }

    void GPUSceneContext::FlushTextureDescriptors() {
        vector<vk::WriteDescriptorSet> descriptorWrites;
        vector<vk::DescriptorImageInfo> descriptorImageInfos;
        vk::WriteDescriptorSet descriptorWrite;
        descriptorWrite.dstSet = textureDescriptorSet;
        descriptorWrite.dstBinding = 0;

        for (size_t offset = 0; offset < texturesToFlush.size();) {
            uint32 descriptorCount = 0;
            auto firstElement = texturesToFlush[offset];
            do {
                const auto &tex = textures[firstElement + descriptorCount];
                descriptorImageInfos.emplace_back(tex->DefaultSampler(), *tex, tex->Image()->LastLayout());
                descriptorCount++;
            } while (++offset < texturesToFlush.size() && texturesToFlush[offset] == firstElement + descriptorCount);

            descriptorWrite.dstArrayElement = firstElement;
            descriptorWrite.descriptorCount = descriptorCount;
            descriptorWrite.pImageInfo = &descriptorImageInfos.back() + 1 - descriptorCount;
            descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites.push_back(descriptorWrite);
        }

        device->updateDescriptorSets(descriptorWrites, {});
        texturesToFlush.clear();
    }
} // namespace sp::vulkan
