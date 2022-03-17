#pragma once

#include "graphics/core/Texture.hh"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

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

    struct ImageCreateInfo {
        vk::ImageCreateFlags flags = {};
        vk::ImageType imageType = vk::ImageType::e2D;
        vk::Format format = vk::Format::eUndefined;
        vk::Extent3D extent = {};
        uint32 mipLevels = 0; // defaults to 1 if genMipmap is false, defaults to CalculateMipmapLevels otherwise
        uint32 arrayLayers = 1;
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
        vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
        vk::ImageUsageFlags usage = {};
        vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined;

        bool genMipmap = false;
        std::vector<double> factor;
        std::vector<vk::Format> formats; // fill only if using eMutableFormat flag

        vk::ImageCreateInfo GetVkCreateInfo() const {
            vk::ImageCreateInfo ci(flags,
                imageType,
                format,
                extent,
                mipLevels,
                arrayLayers,
                samples,
                tiling,
                usage,
                vk::SharingMode::eExclusive,
                {},
                {},
                initialLayout);
            return ci;
        }

        vk::ImageFormatListCreateInfo GetVkFormatList() const {
            return {(uint32)formats.size(), formats.data()};
        }
    };

    class Image : public UniqueMemory {
    public:
        Image();
        ~Image();

        // Allocates storage for the image, destructor destroys image
        Image(vk::ImageCreateInfo imageInfo,
            VmaAllocationCreateInfo allocInfo,
            VmaAllocator allocator,
            vk::ImageUsageFlags declaredUsage);

        // Creates an image reference, destructor does not destroy the image
        Image(vk::Image image, vk::Format format, vk::Extent3D extent, uint32 mipLevels = 1, uint32 arrayLayers = 1)
            : UniqueMemory(VK_NULL_HANDLE), image(image), format(format), extent(extent), mipLevels(mipLevels),
              arrayLayers(arrayLayers) {}

        Image(vk::Image image, vk::Format format, vk::Extent2D extent, uint32 mipLevels = 1, uint32 arrayLayers = 1)
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

        uint32 MipLevels() const {
            return mipLevels;
        }

        uint32 ArrayLayers() const {
            return arrayLayers;
        }

        vk::ImageLayout LastLayout() const {
            return lastLayout;
        }

        Access LastAccess() const {
            return lastAccess;
        }

        vk::ImageUsageFlags Usage() const {
            return usage;
        }

        // Creating an image may add usage flags that are used only during image creation.
        // This returns only the usage flags that will be used outside image creation.
        vk::ImageUsageFlags DeclaredUsage() const {
            return declaredUsage;
        }

        void SetLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
        void SetAccess(Access oldAccess, Access newAccess);

    private:
        vk::Image image;
        vk::Format format;
        vk::Extent3D extent;
        uint32 mipLevels = 0, arrayLayers = 0;
        vk::ImageLayout lastLayout = vk::ImageLayout::eUndefined;
        Access lastAccess = Access::None;
        vk::ImageUsageFlags usage = {}, declaredUsage = {};
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
        vk::ImageAspectFlags aspectMask; // defaults to all aspects of format
        vk::Sampler defaultSampler = VK_NULL_HANDLE;
        vk::ImageUsageFlags usage = {}; // defaults to image's usage
    };

    class ImageView final : public WrappedUniqueHandle<vk::ImageView>, public GpuTexture, public HasUniqueID {
    public:
        ImageView() {}

        // Creates a view to an image, retaining a reference to the image while the view is alive
        ImageView(vk::UniqueImageView &&view, const ImageViewCreateInfo &info = {}) : info(info) {
            extent = info.image->Extent();
            for (uint32 i = 0; i < info.baseMipLevel; i++) {
                extent.width = (extent.width + 1) / 2;
                extent.height = (extent.height + 1) / 2;
                extent.depth = (extent.depth + 1) / 2;
            }
            uniqueHandle = std::move(view);
        }

        const ImagePtr &Image() const {
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

        vk::Sampler DefaultSampler() const {
            return info.defaultSampler;
        }

        uint32 BaseMipLevel() const {
            return info.baseMipLevel;
        }

        uint32 MipLevels() const {
            return info.mipLevelCount;
        }

        uint32 BaseArrayLayer() const {
            return info.baseArrayLayer;
        }

        uint32 ArrayLayers() const {
            return info.arrayLayerCount;
        }

        vk::ImageViewType ViewType() const {
            return info.viewType;
        }

        ImageViewCreateInfo CreateInfo() const {
            return info;
        }

        virtual int GetWidth() const override {
            return extent.width;
        }

        virtual int GetHeight() const override {
            return extent.height;
        }

        virtual uintptr_t GetHandle() const override {
            return reinterpret_cast<uintptr_t>(this);
        }

        static ImageView *FromHandle(uintptr_t handle) {
            return reinterpret_cast<ImageView *>(handle);
        }

    private:
        ImageViewCreateInfo info;
        vk::Extent3D extent;
    };

    vk::Format FormatFromTraits(uint32 components, uint32 bits, bool preferSrgb, bool logErrors = true);
    vk::ImageAspectFlags FormatToAspectFlags(vk::Format format);
    uint32 FormatComponentCount(vk::Format format);
    uint32 FormatByteSize(vk::Format format);
    bool FormatIsSRGB(vk::Format format);
    vk::Format FormatSRGBToUnorm(vk::Format format);
    uint32 CalculateMipmapLevels(vk::Extent3D extent);
    vk::SamplerCreateInfo GLSamplerToVKSampler(int minFilter, int magFilter, int wrapS, int wrapT, int wrapR);
} // namespace sp::vulkan
