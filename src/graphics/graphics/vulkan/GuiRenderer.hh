#pragma once

#include "Common.hh"
#include "Memory.hh"
#include "ecs/components/View.hh"

namespace sp {
    class GuiManager;
} // namespace sp

namespace sp::vulkan {
    class DeviceContext;
    struct Buffer;
    struct VertexLayout;

    class GuiRenderer : public NonCopyable {
    public:
        GuiRenderer(DeviceContext &device, GuiManager &manager);
        void Render(const CommandContextPtr &cmd, vk::Rect2D viewport);

    private:
        double lastTime = 0.0;

        DeviceContext &device;
        GuiManager &manager;

        unique_ptr<VertexLayout> vertexLayout;
        BufferPtr vertexBuffer, indexBuffer;
        ImageViewPtr fontView;

        // TODO: get rid of all these
        BufferPtr fontBuf;
        vk::UniqueSemaphore transferComplete, graphicsTransitionComplete;
    };
} // namespace sp::vulkan
