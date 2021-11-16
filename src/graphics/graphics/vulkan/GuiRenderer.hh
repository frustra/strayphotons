#pragma once

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
        const std::string &Name() const;

        GuiManager *Manager() const {
            return &manager;
        }

    private:
        double lastTime = 0.0;

        DeviceContext &device;
        GuiManager &manager;

        unique_ptr<VertexLayout> vertexLayout;
        BufferPtr vertexBuffer, indexBuffer;
        ImageViewPtr fontView;
    };
} // namespace sp::vulkan
