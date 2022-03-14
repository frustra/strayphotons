#include "VisualizeBuffer.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::renderer {
    ResourceID VisualizeBuffer(RenderGraph &graph, ResourceID sourceID, uint32 arrayLayer) {
        ResourceID outputID;
        graph.AddPass("VisualizeBuffer")
            .Build([&](PassBuilder &builder) {
                builder.Read(sourceID, Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveRenderTarget(sourceID);
                desc.format = vk::Format::eR8G8B8A8Srgb;
                outputID = builder.OutputColorAttachment(0, "", desc, {LoadOp::DontCare, StoreOp::Store}).id;
            })
            .Execute([sourceID, arrayLayer](Resources &resources, CommandContext &cmd) {
                auto target = resources.GetRenderTarget(sourceID);
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
