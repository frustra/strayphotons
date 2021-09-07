#pragma once

#include "Common.hh"
#include "Memory.hh"
#include "ecs/components/View.hh"

namespace sp {
    class GuiManager;
} // namespace sp

namespace sp::vulkan {
    class DeviceContext;
    struct UniqueBuffer;
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
        UniqueBuffer vertexBuffer, indexBuffer;
        UniqueImage fontImage;
        vk::UniqueImageView fontView;

        // TODO: get rid of all these
        UniqueBuffer fontBuf;
        vk::UniqueSemaphore transferComplete, graphicsTransitionComplete;
        vk::UniqueSampler fontSampler;
        vk::UniqueDescriptorSetLayout descriptorSetLayout;
        vk::UniqueDescriptorPool descriptorPool;
        vector<vk::DescriptorSet> descriptorSets;
    };
} // namespace sp::vulkan
