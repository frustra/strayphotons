#include "Image.hh"

#include "core/Logging.hh"

#include <tiny_gltf.h>

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

    vk::Format FormatFromTraits(uint32 components, uint32 bits, bool preferSrgb, bool logErrors) {
        if (bits != 8 && bits != 16) {
            if (logErrors) Errorf("can't infer format with bits=%d", bits);
            return vk::Format::eUndefined;
        }

        if (components == 4) {
            if (bits == 16) return vk::Format::eR16G16B16A16Snorm;
            return preferSrgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Snorm;
        } else if (components == 3) {
            if (bits == 16) return vk::Format::eR16G16B16Snorm;
            return preferSrgb ? vk::Format::eR8G8B8Srgb : vk::Format::eR8G8B8Snorm;
        } else if (components == 2) {
            if (bits == 16) return vk::Format::eR16G16Snorm;
            return preferSrgb ? vk::Format::eR8G8Srgb : vk::Format::eR8G8Snorm;
        } else if (components == 1) {
            if (bits == 16) return vk::Format::eR16Snorm;
            return preferSrgb ? vk::Format::eR8Srgb : vk::Format::eR8Snorm;
        } else {
            if (logErrors) Errorf("can't infer format with components=%d", components);
        }
        return vk::Format::eUndefined;
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

    static vk::SamplerAddressMode GLWrapToVKAddressMode(int wrap) {
        switch (wrap) {
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
            return vk::SamplerAddressMode::eClampToEdge;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            return vk::SamplerAddressMode::eMirroredRepeat;
        default:
            return vk::SamplerAddressMode::eRepeat;
        }
    }

    vk::SamplerCreateInfo GLSamplerToVKSampler(int minFilter, int magFilter, int wrapS, int wrapT, int wrapR) {
        vk::SamplerCreateInfo info;

        switch (magFilter) {
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            info.magFilter = vk::Filter::eLinear;
        default:
            info.minFilter = vk::Filter::eNearest;
        }

        switch (minFilter) {
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            info.minFilter = vk::Filter::eLinear;
            break;
        default:
            info.magFilter = vk::Filter::eNearest;
        }

        switch (minFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            info.mipmapMode = vk::SamplerMipmapMode::eLinear;
            break;
        default:
            info.mipmapMode = vk::SamplerMipmapMode::eNearest;
        }

        info.addressModeU = GLWrapToVKAddressMode(wrapS);
        info.addressModeV = GLWrapToVKAddressMode(wrapT);
        info.addressModeW = GLWrapToVKAddressMode(wrapR);
        return info;
    }
} // namespace sp::vulkan
