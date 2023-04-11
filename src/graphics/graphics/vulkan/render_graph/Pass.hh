#pragma once

#include "core/InlineVector.hh"
#include "graphics/vulkan/core/RenderPass.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/Resources.hh"

#include <variant>

namespace sp::vulkan::render_graph {
    struct ResourceIDAccess {
        ResourceID id;
        Access access;

        bool IsWrite() const {
            return AccessIsWrite(access);
        }
    };

    struct ResourceIDFutureAccess {
        ResourceID id;
        Access access;
        int framesFromNow;
    };

    struct AttachmentInfo {
        AttachmentInfo() {}
        AttachmentInfo(LoadOp loadOp, StoreOp storeOp) : loadOp(loadOp), storeOp(storeOp) {}
        LoadOp loadOp = LoadOp::DontCare;
        StoreOp storeOp = StoreOp::DontCare;
        vk::ClearColorValue clearColor = {};
        vk::ClearDepthStencilValue clearDepthStencil = {1.0f, 0};
        uint32 arrayIndex = ~0u; // if the attachment is an array image, this can be set to render to a specific index

        void SetClearColor(glm::vec4 clear) {
            std::array<float, 4> clearValues = {clear.r, clear.g, clear.b, clear.a};
            clearColor = {clearValues};
        }

    private:
        friend class RenderGraph;
        friend class PassBuilder;
        ResourceID resourceID = InvalidResource;
    };

    class Pass {
    public:
        Pass(string_view name) : name(name) {}

        void AddAccess(ResourceID id, Access access) {
            accesses.push_back({id, access});
        }
        void AddFutureRead(ResourceID id, Access access, int framesFromNow) {
            futureReads.push_back({id, access, framesFromNow});
        }

        bool HasExecute() const {
            return executeFunc.index() > 0;
        }
        bool ExecutesWithCommandContext() const {
            return executeFunc.index() == 1;
        }
        bool ExecutesWithDeviceContext() const {
            return executeFunc.index() == 2;
        }
        void Execute(Resources &resources, CommandContext &cmd) const {
            std::get<1>(executeFunc)(resources, cmd);
        }
        void Execute(Resources &resources, DeviceContext &device) const {
            std::get<2>(executeFunc)(resources, device);
        }

    private:
        friend class RenderGraph;
        friend class PassBuilder;
        string_view name;
        InlineVector<ResourceIDAccess, 128> accesses;
        vector<ResourceIDFutureAccess> futureReads;
        std::array<AttachmentInfo, MAX_COLOR_ATTACHMENTS + 1> attachments;
        bool active = false, required = false;
        uint8 primaryAttachmentIndex = 0;
        bool isRenderPass = false;
        bool flushCommands = false; // true will submit pending command buffers

        std::variant<std::monostate,
            std::function<void(Resources &, CommandContext &)>,
            std::function<void(Resources &, DeviceContext &)>>
            executeFunc;

        InlineVector<uint8, MAX_RESOURCE_SCOPE_DEPTH> scopes;
    };
} // namespace sp::vulkan::render_graph
