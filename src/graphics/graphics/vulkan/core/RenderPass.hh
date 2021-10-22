#pragma once

#include "core/Hashing.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Image.hh"

#include <robin_hood.h>

namespace sp::vulkan {
    const size_t MAX_COLOR_ATTACHMENTS = 4;

    struct RenderPassState {
        uint32 colorAttachmentCount = 0;

        vk::Format colorFormats[MAX_COLOR_ATTACHMENTS] = {};
        vk::Format depthStencilFormat = {};

        // depth/stencil bit is at DEPTH_STENCIL_INDEX, not the actual depth attachment index
        uint32 clearAttachments = 0;
        uint32 loadAttachments = 0;
        uint32 storeAttachments = 0;
        uint32 multiviewAttachments = 0;
        uint32 multiviewCorrelation = 0;

        static const uint32 DEPTH_STENCIL_INDEX = 31;

        bool HasDepthStencil() const {
            return depthStencilFormat != vk::Format::eUndefined;
        }

        void SetLoadStore(uint32 index, LoadOp loadOp, StoreOp storeOp) {
            uint32 attachmentBit = 1 << index;
            if (loadOp == LoadOp::Clear) {
                clearAttachments |= attachmentBit;
            } else {
                clearAttachments &= ~attachmentBit;
            }
            if (loadOp == LoadOp::Load) {
                loadAttachments |= attachmentBit;
            } else {
                loadAttachments &= ~attachmentBit;
            }
            if (storeOp == StoreOp::Store) {
                storeAttachments |= attachmentBit;
            } else {
                storeAttachments &= ~attachmentBit;
            }
        }

        vk::AttachmentLoadOp LoadOp(uint32 index) const {
            uint32 attachmentBit = 1 << index;
            if (clearAttachments & attachmentBit) return vk::AttachmentLoadOp::eClear;
            if (loadAttachments & attachmentBit) return vk::AttachmentLoadOp::eClear;
            return vk::AttachmentLoadOp::eDontCare;
        }

        vk::AttachmentStoreOp StoreOp(uint32 index) const {
            uint32 attachmentBit = 1 << index;
            if (storeAttachments & attachmentBit) return vk::AttachmentStoreOp::eStore;
            return vk::AttachmentStoreOp::eDontCare;
        }

        bool ShouldClear(uint32 index) const {
            uint32 attachmentBit = 1 << index;
            return clearAttachments & attachmentBit;
        }

        void SetMultiview(uint32 index, bool enabled, bool correlated) {
            uint32 attachmentBit = 1 << index;
            if (enabled) {
                multiviewAttachments |= attachmentBit;
            } else {
                multiviewAttachments &= ~attachmentBit;
            }

            // Vulkan allows you to specify an arbitrary number of correlation masks, to indicate that
            // multiple subsets of attachments are spatially correlated in different ways. We currently support a single
            // correlation mask, since all our attachments are spatially correlated in the same way.
            if (enabled && correlated) {
                multiviewCorrelation |= attachmentBit;
            } else {
                multiviewCorrelation &= ~attachmentBit;
            }
        }

        uint32 MoveStateDepthBitToActualDepthBit(uint32 mask) const {
            uint32 stateDepthBit = 1u << DEPTH_STENCIL_INDEX;
            uint32 actualDepthBit = 1u << colorAttachmentCount;
            if (mask & stateDepthBit) mask |= actualDepthBit;
            return mask & ~stateDepthBit;
        }

        uint32 MultiviewMask() const {
            return MoveStateDepthBitToActualDepthBit(multiviewAttachments);
        }

        uint32 CorrelationMask() const {
            return MoveStateDepthBitToActualDepthBit(multiviewCorrelation);
        }
    };

    struct RenderPassInfo {
        RenderPassState state;
        ImageViewPtr colorAttachments[MAX_COLOR_ATTACHMENTS] = {};
        ImageViewPtr depthStencilAttachment;

        vk::ClearColorValue clearColors[MAX_COLOR_ATTACHMENTS] = {};
        vk::ClearDepthStencilValue clearDepthStencil = {1.0f, 0};

        void PushColorAttachment(const ImageViewPtr &view,
            LoadOp loadOp,
            StoreOp storeOp,
            vk::ClearColorValue clear = {}) {
            Assert(state.colorAttachmentCount < MAX_COLOR_ATTACHMENTS, "too many color attachments");
            uint32 index = state.colorAttachmentCount++;
            SetColorAttachment(index, view, loadOp, storeOp, clear);
        }

        void PushColorAttachment(const ImageViewPtr &view, LoadOp loadOp, StoreOp storeOp, glm::vec4 clear) {
            std::array<float, 4> clearValues = {clear.r, clear.g, clear.b, clear.a};
            PushColorAttachment(view, loadOp, storeOp, clearValues);
        }

        void SetColorAttachment(uint32 index,
            const ImageViewPtr &view,
            LoadOp loadOp,
            StoreOp storeOp,
            vk::ClearColorValue clear = {}) {
            state.SetLoadStore(index, loadOp, storeOp);
            state.colorFormats[index] = view->Format();
            clearColors[index] = clear;
            colorAttachments[index] = view;

            state.SetMultiview(index, view->ArrayLayers() > 1, true);
        }

        void SetDepthStencilAttachment(const ImageViewPtr &view,
            LoadOp loadOp,
            StoreOp storeOp,
            vk::ClearDepthStencilValue clear = {1.0f, 0}) {
            state.SetLoadStore(RenderPassState::DEPTH_STENCIL_INDEX, loadOp, storeOp);
            state.depthStencilFormat = view->Format();
            clearDepthStencil = clear;
            depthStencilAttachment = view;

            state.SetMultiview(RenderPassState::DEPTH_STENCIL_INDEX, view->ArrayLayers() > 1, true);
        }

        bool HasDepthStencil() const {
            return state.HasDepthStencil();
        }
    };

    class RenderPass : public WrappedUniqueHandle<vk::RenderPass> {
    public:
        RenderPass(DeviceContext &device, const RenderPassInfo &info);

        // Updates the cached layout of the framebuffer attachment images
        void RecordImplicitImageLayoutTransitions(const RenderPassInfo &info);

        uint32 ColorAttachmentCount() const {
            return state.colorAttachmentCount;
        }

    private:
        RenderPassState state;
        std::array<vk::ImageLayout, MAX_COLOR_ATTACHMENTS + 1> initialLayouts, finalLayouts;
    };

    class Framebuffer : public WrappedUniqueHandle<vk::Framebuffer> {
    public:
        Framebuffer(DeviceContext &device, const RenderPassInfo &info);

        shared_ptr<RenderPass> GetRenderPass() const {
            return renderPass;
        }

        vk::Extent2D Extent() const {
            return extent;
        }

    private:
        shared_ptr<RenderPass> renderPass;
        vk::Extent2D extent;
    };

    class RenderPassManager : public NonCopyable {
    public:
        RenderPassManager(DeviceContext &device);

        using RenderPassKey = HashKey<RenderPassState>;

        shared_ptr<RenderPass> GetRenderPass(const RenderPassInfo &info);

    private:
        DeviceContext &device;

        robin_hood::unordered_map<RenderPassKey, shared_ptr<RenderPass>, RenderPassKey::Hasher> renderPasses;
    };

    class FramebufferManager : public NonCopyable {
    public:
        FramebufferManager(DeviceContext &device);

        struct FramebufferKeyData {
            RenderPassState renderPass;

            // TODO: use UniqueID to avoid collisions upon pointer reuse
            vk::ImageView imageViews[MAX_COLOR_ATTACHMENTS + 1];

            vk::Extent2D extents[MAX_COLOR_ATTACHMENTS + 1];
        };

        using FramebufferKey = HashKey<FramebufferKeyData>;

        shared_ptr<Framebuffer> GetFramebuffer(const RenderPassInfo &info);

    private:
        DeviceContext &device;

        robin_hood::unordered_map<FramebufferKey, shared_ptr<Framebuffer>, FramebufferKey::Hasher> framebuffers;
    };
} // namespace sp::vulkan
