#pragma once

#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

struct ImFontAtlas;

namespace sp {
    class GuiManager;
} // namespace sp

namespace sp::vulkan {
    struct VertexLayout;

    class GuiRenderer : public NonCopyable {
    public:
        GuiRenderer(DeviceContext &device);
        void Render(GuiManager &manager, CommandContext &cmd, vk::Rect2D viewport);
        void Tick();

    private:
        double lastTime = 0.0, deltaTime;

        DeviceContext &device;

        unique_ptr<VertexLayout> vertexLayout;

        shared_ptr<ImFontAtlas> fontAtlas;
        AsyncPtr<ImageView> fontView;
    };
} // namespace sp::vulkan
