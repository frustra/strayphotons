#include "VisualizeBuffer.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::renderer {
    ResourceID VisualizeBuffer(RenderGraph &graph, ResourceID sourceID, uint32 arrayLayer) {
        ResourceID targetID = InvalidResource, outputID;
        graph.AddPass("VisualizeBuffer")
            .Build([&](PassBuilder &builder) {
                auto &res = builder.TextureRead(sourceID);
                targetID = res.id;
                auto desc = res.DeriveRenderTarget();
                desc.format = vk::Format::eR8G8B8A8Srgb;
                outputID = builder.OutputColorAttachment(0, "", desc, {LoadOp::DontCare, StoreOp::Store}).id;
            })
            .Execute([targetID, arrayLayer](Resources &resources, CommandContext &cmd) {
                auto target = resources.GetRenderTarget(targetID);
                ImageViewPtr source;
                if (target->Desc().arrayLayers > 1 && arrayLayer != ~0u && arrayLayer < target->Desc().arrayLayers) {
                    source = target->LayerImageView(arrayLayer);
                } else {
                    source = target->ImageView();
                }

                if (source->ViewType() == vk::ImageViewType::e2DArray) {
                    cmd.SetShaders("screen_cover.vert", "visualize_buffer_2d_array.frag");

                    struct {
                        float layer = 0;
                    } push;
                    cmd.PushConstants(push);
                } else {
                    cmd.SetShaders("screen_cover.vert", "visualize_buffer_2d.frag");
                }

                auto format = source->Format();
                uint32 comp = FormatComponentCount(format);
                uint32 swizzle = 0b11000000; // rrra
                if (comp > 1) {
                    swizzle = 0b11100100; // rgba
                }
                cmd.SetShaderConstant(ShaderStage::Fragment, 0, swizzle);

                cmd.SetImageView(0, 0, source);
                cmd.Draw(3);
            });
        return outputID;
    }
} // namespace sp::vulkan::renderer
