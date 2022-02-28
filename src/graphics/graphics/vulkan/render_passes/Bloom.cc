#include "Bloom.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"

namespace sp::vulkan::renderer {
    static CVar<float> CVarBloomScale("r.BloomScale", 0.15f, "Bloom scale");

    void AddBloom(RenderGraph &graph) {
        auto sourceID = graph.LastOutputID();

        graph.BeginScope("BloomBlur");
        auto blurY1 = renderer::AddGaussianBlur1D(graph, sourceID, glm::ivec2(0, 1), 1, CVarBloomScale.Get());
        auto blur1 = renderer::AddGaussianBlur1D(graph, blurY1, glm::ivec2(1, 0), 2);
        auto blurY2 = renderer::AddGaussianBlur1D(graph, blur1, glm::ivec2(0, 1), 1);
        auto blur2 = renderer::AddGaussianBlur1D(graph, blurY2, glm::ivec2(1, 0), 1);
        graph.EndScope();

        graph.AddPass("BloomCombine")
            .Build([&](rg::PassBuilder &builder) {
                auto source = builder.ShaderRead(sourceID);
                auto desc = source.DeriveRenderTarget();
                builder.OutputColorAttachment(0, "Bloom", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ShaderRead(blur1);
                builder.ShaderRead(blur2);
            })
            .Execute([sourceID, blur1, blur2](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "bloom_combine.frag");
                cmd.SetTexture(0, 0, resources.GetRenderTarget(sourceID)->ImageView());
                cmd.SetTexture(0, 1, resources.GetRenderTarget(blur1)->ImageView());
                cmd.SetTexture(0, 2, resources.GetRenderTarget(blur2)->ImageView());
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
