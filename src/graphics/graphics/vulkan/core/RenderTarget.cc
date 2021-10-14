#include "RenderTarget.hh"

#include "core/Logging.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    RenderTarget::~RenderTarget() {
        if (poolIndex != ~0u) {
            Debugf("Destroying render target %d, size=%dx%d", poolIndex, desc.extent.width, desc.extent.height);
        }
    }

    RenderTargetPtr RenderTargetManager::Get(const RenderTargetDesc &desc) {
        for (auto &elemRef : pool) {
            if (elemRef.use_count() <= 1 && elemRef->desc == desc) {
                elemRef->unusedFrames = 0;
                return elemRef;
            }
        }

        Debugf("Creating render target %d, size=%dx%d", pool.size(), desc.extent.width, desc.extent.height);

        ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent = desc.extent;
        imageInfo.arrayLayers = desc.arrayLayers;
        imageInfo.format = desc.format;
        imageInfo.usage = desc.usage;

        ImageViewCreateInfo viewInfo;
        viewInfo.defaultSampler = device.GetSampler(SamplerType::BilinearTiled);

        auto imageView = device.CreateImageAndView(imageInfo, viewInfo);
        auto ptr = make_shared<RenderTarget>(desc, imageView, pool.size());

        pool.push_back(ptr);
        return ptr;
    }

    void RenderTargetManager::TickFrame() {
        erase_if(pool, [&](auto &elemRef) {
            if (elemRef.use_count() > 1) {
                elemRef->unusedFrames = 0;
                return false;
            }
            return elemRef->unusedFrames++ > 4;
        });
    }
} // namespace sp::vulkan
