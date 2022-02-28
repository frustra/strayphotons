#include "Bloom.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"

namespace sp::vulkan::renderer {
    static CVar<float> CVarBloomScale("r.BloomScale", 0.15f, "Bloom scale");

    void AddBloom(RenderGraph &graph) {
        graph.BeginScope("BloomBlur");
        auto sourceID = graph.LastOutputID();
        auto blurY1 = renderer::AddGaussianBlur(graph, sourceID, glm::ivec2(0, 1), 1, CVarBloomScale.Get());
        auto blurX1 = renderer::AddGaussianBlur(graph, blurY1, glm::ivec2(1, 0), 2);
        auto blurY2 = renderer::AddGaussianBlur(graph, blurX1, glm::ivec2(0, 1), 1);
        auto blurX2 = renderer::AddGaussianBlur(graph, blurY2, glm::ivec2(1, 0), 1);
        graph.EndScope();

        graph.AddPass("BloomCombine")
            .Build([&](rg::PassBuilder &builder) {
                auto source = builder.ShaderRead(sourceID);
                auto desc = source.DeriveRenderTarget();
                builder.OutputColorAttachment(0, "Bloom", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ShaderRead(blurX1);
                builder.ShaderRead(blurX2);
            })
            .Execute([sourceID, blurX1, blurX2](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "bloom_combine.frag");
                cmd.SetTexture(0, 0, resources.GetRenderTarget(sourceID)->ImageView());
                cmd.SetTexture(0, 1, resources.GetRenderTarget(blurX1)->ImageView());
                cmd.SetTexture(0, 2, resources.GetRenderTarget(blurX2)->ImageView());
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
