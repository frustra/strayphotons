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

        // each bit represents a specific array layer to enable rendering to
        uint32 multiviewMask = 0;
        // Vulkan allows you to specify an arbitrary number of correlation masks, to indicate that
        // multiple subsets of attachments are spatially correlated in different ways. We currently support a single
        // correlation mask, since all our attachments are spatially correlated in the same way.
        uint32 multiviewCorrelationMask = 0;

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
            if (loadAttachments & attachmentBit) return vk::AttachmentLoadOp::eLoad;
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
    };

    struct RenderPassInfo {
        RenderPassState state;
        ImageViewPtr colorAttachments[MAX_COLOR_ATTACHMENTS] = {};
        ImageViewPtr depthStencilAttachment;
        uint32 minAttachmentLayers = ~0u;

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

            EnableMultiviewForAllLayers(view);
        }

        void SetDepthStencilAttachment(const ImageViewPtr &view,
            LoadOp loadOp,
            StoreOp storeOp,
            vk::ClearDepthStencilValue clear = {1.0f, 0}) {
            state.SetLoadStore(RenderPassState::DEPTH_STENCIL_INDEX, loadOp, storeOp);
            state.depthStencilFormat = view->Format();
            clearDepthStencil = clear;
            depthStencilAttachment = view;

            EnableMultiviewForAllLayers(view);
        }

        void EnableMultiviewForAllLayers(const ImageViewPtr &view) {
            uint32 layers = view->ArrayLayers();
            if (layers < minAttachmentLayers) { minAttachmentLayers = layers; }

            state.multiviewMask = 0;

            if (minAttachmentLayers >= 2) {
                for (uint32 i = 0; i < minAttachmentLayers; i++) {
                    state.multiviewMask |= (1 << i);
                }
            }

            // Assume all layers are spatially correlated.
            // This is true for VR views, but not for shadow maps.
            state.multiviewCorrelationMask = state.multiviewMask;
        }

        bool HasDepthStencil() const {
            return state.HasDepthStencil();
        }
    };

    class RenderPass : public WrappedUniqueHandle<vk::RenderPass>, public HasUniqueID {
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
            UniqueID imageViewIDs[MAX_COLOR_ATTACHMENTS + 1];
            vk::Extent2D extents[MAX_COLOR_ATTACHMENTS + 1];
        };

        using FramebufferKey = HashKey<FramebufferKeyData>;

        shared_ptr<Framebuffer> GetFramebuffer(const RenderPassInfo &info);

    private:
        DeviceContext &device;

        robin_hood::unordered_map<FramebufferKey, shared_ptr<Framebuffer>, FramebufferKey::Hasher> framebuffers;
    };
} // namespace sp::vulkan
