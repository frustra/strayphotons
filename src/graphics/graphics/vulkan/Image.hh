#pragma once

#include "Common.hh"
#include "Memory.hh"
#include "graphics/core/Texture.hh"

namespace sp::vulkan {
    enum class LoadOp {
        DontCare,
        Clear,
        Load,
    };

    enum class StoreOp {
        DontCare,
        Store,
    };

    struct Image : public UniqueMemory {
        Image();

        // Allocates storage for the image, destructor destroys image
        Image(vk::ImageCreateInfo imageInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator);
        ~Image();

        // Creates an image reference, destructor does not destroy the image
        Image(vk::Image image, vk::Format format, vk::Extent3D extent)
            : UniqueMemory(VK_NULL_HANDLE), image(image), format(format), extent(extent) {}

        Image(vk::Image image, vk::Format format, vk::Extent2D extent)
            : Image(image, format, vk::Extent3D(extent, 1)) {}

        vk::Image operator*() const {
            return image;
        }

        operator vk::Image() const {
            return image;
        }

        vk::Format Format() const {
            return format;
        }

        vk::Extent3D Extent() const {
            return extent;
        }

    private:
        vk::Image image;
        vk::Format format;
        vk::Extent3D extent;
    };

    struct ImageViewCreateInfo {
        ImagePtr image;
        vk::Format format = vk::Format::eUndefined; // infer format from image
        vk::ImageViewType viewType = vk::ImageViewType::e2D;
        vk::ComponentMapping mapping = {};
        vk::ImageLayout swapchainLayout = vk::ImageLayout::eUndefined; // set only if this is a swapchain image
        uint32_t baseMipLevel = 0;
        uint32_t mipLevelCount = VK_REMAINING_MIP_LEVELS; // all mips after the base level are included
        uint32_t baseArrayLayer = 0;
        uint32_t arrayLayerCount = VK_REMAINING_ARRAY_LAYERS; // all layers after the base layer are included
    };

    struct ImageView : public WrappedUniqueHandle<vk::ImageView>, public GpuTexture {
        ImageView() {}

        // Creates a view to an image, retaining a reference to the image while the view is alive
        ImageView(vk::UniqueImageView &&view, const ImageViewCreateInfo &info = {})
            : info(info), extent(info.image->Extent()) {
            uniqueHandle = std::move(view);
        }

        ImagePtr Image() const {
            return info.image;
        }

        vk::Format Format() const {
            return info.format;
        }

        vk::Extent3D Extent() const {
            return extent;
        }

        vk::ImageLayout SwapchainLayout() const {
            return info.swapchainLayout;
        }

        bool IsSwapchain() const {
            return info.swapchainLayout != vk::ImageLayout::eUndefined;
        }

        virtual uintptr_t GetHandle() const override {
            return uintptr_t(this);
        }

        virtual int GetWidth() const override {
            return extent.width;
        }

        virtual int GetHeight() const override {
            return extent.height;
        }

    private:
        ImageViewCreateInfo info;
        vk::Extent3D extent;
    };

    vk::ImageAspectFlags FormatToAspectFlags(vk::Format format);
    uint32 CalculateMipmapLevels(vk::Extent3D extent);
} // namespace sp::vulkan
