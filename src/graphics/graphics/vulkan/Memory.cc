#define VMA_IMPLEMENTATION
#include "Memory.hh"

#include "core/Common.hh"

namespace sp::vulkan {
    vk::Result UniqueMemory::Map(void **data) {
        return static_cast<vk::Result>(vmaMapMemory(allocator, allocation, data));
    }

    void UniqueMemory::Unmap() {
        vmaUnmapMemory(allocator, allocation);
    }

    UniqueBuffer::UniqueBuffer() : UniqueMemory(VK_NULL_HANDLE) {
        Release();
    }

    UniqueBuffer::UniqueBuffer(vk::BufferCreateInfo bufferInfo,
                               VmaAllocationCreateInfo allocInfo,
                               VmaAllocator allocator)
        : UniqueMemory(allocator), bufferInfo(bufferInfo) {

        VkBufferCreateInfo vkBufferInfo = bufferInfo;
        VkBuffer vkBuffer;

        auto result = vmaCreateBuffer(allocator, &vkBufferInfo, &allocInfo, &vkBuffer, &allocation, nullptr);
        AssertVKSuccess(result, "creating command buffer");
        buffer = vkBuffer;
    }

    UniqueBuffer::UniqueBuffer(UniqueBuffer &&other) : UniqueMemory(other.allocator) {
        *this = std::move(other);
    }

    UniqueBuffer::~UniqueBuffer() {
        Destroy();
    }

    UniqueBuffer &UniqueBuffer::operator=(UniqueBuffer &&other) {
        Destroy();
        allocator = other.allocator;
        allocation = other.allocation;
        buffer = other.buffer;
        bufferInfo = other.bufferInfo;
        other.Release();
        return *this;
    }

    void UniqueBuffer::Destroy() {
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, buffer, allocation);
            Release();
        }
    }

    void UniqueBuffer::Release() {
        allocator = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        buffer = VK_NULL_HANDLE;
        bufferInfo = vk::BufferCreateInfo();
    }

    UniqueImage::UniqueImage() : UniqueMemory(VK_NULL_HANDLE) {
        Release();
    }

    UniqueImage::UniqueImage(vk::ImageCreateInfo imageInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator)
        : UniqueMemory(allocator), imageInfo(imageInfo) {

        VkImageCreateInfo vkImageInfo = imageInfo;
        VkImage vkImage;

        auto result = vmaCreateImage(allocator, &vkImageInfo, &allocInfo, &vkImage, &allocation, nullptr);
        AssertVKSuccess(result, "creating image");
        image = vkImage;
    }

    UniqueImage::UniqueImage(UniqueImage &&other) : UniqueMemory(other.allocator) {
        *this = std::move(other);
    }

    UniqueImage::~UniqueImage() {
        Destroy();
    }

    UniqueImage &UniqueImage::operator=(UniqueImage &&other) {
        Destroy();
        allocator = other.allocator;
        allocation = other.allocation;
        image = other.image;
        imageInfo = other.imageInfo;
        other.Release();
        return *this;
    }

    void UniqueImage::Destroy() {
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, image, allocation);
            Release();
        }
    }

    void UniqueImage::Release() {
        allocator = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        image = VK_NULL_HANDLE;
        imageInfo = vk::ImageCreateInfo();
    }

    vk::DeviceSize UniqueImage::Size() {
        return allocation->GetSize();
    }
} // namespace sp::vulkan
