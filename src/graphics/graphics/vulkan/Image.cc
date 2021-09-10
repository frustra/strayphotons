#include "Image.hh"

namespace sp::vulkan {
    Image::Image() : UniqueMemory(VK_NULL_HANDLE) {}

    Image::Image(vk::ImageCreateInfo imageInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator)
        : UniqueMemory(allocator), format(imageInfo.format), extent(imageInfo.extent) {

        VkImageCreateInfo vkImageInfo = imageInfo;
        VkImage vkImage;

        auto result = vmaCreateImage(allocator, &vkImageInfo, &allocInfo, &vkImage, &allocation, nullptr);
        AssertVKSuccess(result, "creating image");
        image = vkImage;
    }

    Image::~Image() {
        if (allocator != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, image, allocation);
        }
    }

    vk::ImageAspectFlags FormatToAspectFlags(vk::Format format) {
        switch (format) {
        case vk::Format::eUndefined:
            return {};

        case vk::Format::eS8Uint:
            return vk::ImageAspectFlagBits::eStencil;

        case vk::Format::eD16UnormS8Uint:
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return vk::ImageAspectFlagBits::eStencil | vk::ImageAspectFlagBits::eDepth;

        case vk::Format::eD16Unorm:
        case vk::Format::eD32Sfloat:
        case vk::Format::eX8D24UnormPack32:
            return vk::ImageAspectFlagBits::eDepth;

        default:
            return vk::ImageAspectFlagBits::eColor;
        }
    }

    uint32 CalculateMipmapLevels(vk::Extent3D extent) {
        uint32 dim = std::max(std::max(extent.width, extent.height), extent.depth);
        if (dim <= 0) return 1;
        uint32 cmp = 31;
        while (!(dim >> cmp))
            cmp--;
        return cmp + 1;
    }
} // namespace sp::vulkan
