#pragma once

#include "assets/Async.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"

struct ImFontAtlas;

namespace sp {
    class GuiContext;
} // namespace sp

namespace sp::vulkan {
    struct VertexLayout;

    class GuiRenderer : public NonCopyable {
    public:
        GuiRenderer(DeviceContext &device);
        void Render(GuiContext &context, CommandContext &cmd, vk::Rect2D viewport);
        void Tick();

    private:
        double lastTime = 0.0, deltaTime;

        std::unique_ptr<VertexLayout> vertexLayout;

        std::shared_ptr<ImFontAtlas> fontAtlas;
        AsyncPtr<ImageView> fontView;
    };
} // namespace sp::vulkan
