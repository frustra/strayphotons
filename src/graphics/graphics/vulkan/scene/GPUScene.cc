#include "GPUScene.hh"

#include "assets/GltfImpl.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    GPUScene::GPUScene(DeviceContext &device) : device(device), workQueue("", 0), textures(device, workQueue) {
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

    void GPUScene::Flush() {
        workQueue.Flush();
        textures.Flush();
    }
} // namespace sp::vulkan
