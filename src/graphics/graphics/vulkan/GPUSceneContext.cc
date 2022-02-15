#include "graphics/vulkan/GPUSceneContext.hh"

#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    GPUSceneContext::GPUSceneContext(DeviceContext &device) : device(device), workQueue("", 0) {
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
    }

    std::pair<TextureIndex, std::future<void>> GPUSceneContext::AddTexture(const ImageCreateInfo &imageInfo,
        const ImageViewCreateInfo &viewInfo,
        const InitialData &data) {
        auto i = AllocateTextureIndex();
        auto imageViewFut = device.CreateImageAndView(imageInfo, viewInfo, data);
        return make_pair(i,
            workQueue.Dispatch<void>(
                [this, i](ImageViewPtr view) {
                    textures[i] = view;
                    texturesToFlush.push_back(i);
                },
                std::move(imageViewFut)));
    }

    TextureIndex GPUSceneContext::AddTexture(const ImageViewPtr &ptr) {
        auto i = AllocateTextureIndex();
        textures[i] = ptr;
        texturesToFlush.push_back(i);
        return i;
    }

    TextureIndex GPUSceneContext::AllocateTextureIndex() {
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

    void GPUSceneContext::ReleaseTexture(TextureIndex i) {
        textures[i].reset();
        freeTextureIndexes.push_back(i);
    }

    void GPUSceneContext::FlushTextureDescriptors() {
        workQueue.Flush();

        vector<vk::WriteDescriptorSet> descriptorWrites;
        vector<vk::DescriptorImageInfo> descriptorImageInfos;
        vk::WriteDescriptorSet descriptorWrite;
        descriptorWrite.dstSet = textureDescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;

        for (auto descriptorIndex : texturesToFlush) {
            const auto &tex = textures[descriptorIndex];
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
} // namespace sp::vulkan
