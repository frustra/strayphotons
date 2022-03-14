#include "Tonemap.hh"

#include "graphics/vulkan/core/CommandContext.hh"

namespace sp::vulkan::renderer {
    void AddTonemap(RenderGraph &graph) {
        graph.AddPass("Tonemap")
            .Build([&](rg::PassBuilder &builder) {
                auto luminanceID = builder.LastOutputID();
                builder.Read(luminanceID, Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveRenderTarget(luminanceID);
                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "TonemappedLuminance", desc, {LoadOp::DontCare, StoreOp::Store});
            })
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "tonemap.frag");
                cmd.SetImageView(0, 0, resources.GetRenderTarget(resources.LastOutputID())->ImageView());
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
