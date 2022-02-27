#include "Blur.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::renderer {
    ResourceID AddGaussianBlur(RenderGraph &graph,
        ResourceID sourceID,
        glm::ivec2 direction,
        uint32 downsample,
        float scale,
        float clip) {

        struct {
            glm::vec2 direction;
            float threshold;
            float scale;
        } constants;

        constants.direction = glm::vec2(direction);
        constants.threshold = clip;
        constants.scale = scale;

        ResourceID destID;
        graph.AddPass("GaussianBlur")
            .Build([&](PassBuilder &builder) {
                auto source = builder.ShaderRead(sourceID);
                auto desc = source.DeriveRenderTarget();
                desc.extent.width = std::max(desc.extent.width / downsample, 1u);
                desc.extent.height = std::max(desc.extent.height / downsample, 1u);
                auto dest = builder.OutputColorAttachment(0, "", desc, {LoadOp::DontCare, StoreOp::Store});
                destID = dest.id;
            })
            .Execute([sourceID, constants](Resources &resources, CommandContext &cmd) {
                auto source = resources.GetRenderTarget(sourceID);

                if (source->Desc().primaryViewType == vk::ImageViewType::e2DArray) {
                    cmd.SetShaders("screen_cover.vert", "gaussian_blur_array.frag");
                } else {
                    cmd.SetShaders("screen_cover.vert", "gaussian_blur.frag");
                }

                cmd.SetTexture(0, 0, source->ImageView());
                cmd.PushConstants(constants);
                cmd.Draw(3);
            });

        return destID;
    }
} // namespace sp::vulkan::renderer
