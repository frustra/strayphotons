#pragma once

#include "ecs/components/View.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

namespace sp {
    class GuiManager;
} // namespace sp

namespace sp::vulkan {
    struct VertexLayout;

    class GuiRenderer : public NonCopyable {
    public:
        GuiRenderer(DeviceContext &device, GuiManager &manager);
        void Render(CommandContext &cmd, vk::Rect2D viewport);

    private:
        double lastTime = 0.0;

        DeviceContext &device;
        GuiManager &manager;

        unique_ptr<VertexLayout> vertexLayout;
        BufferPtr vertexBuffer, indexBuffer;
        ImageViewPtr fontView;
    };
} // namespace sp::vulkan
