/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "VisualizeBuffer.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::renderer {
    static CVar<uint32> CVarWindowViewChannel("r.WindowViewChannel", 0, "A specific channel to view. 0-3 maps to RGBA");

    ResourceID VisualizeBuffer(RenderGraph &graph, ResourceID sourceID, uint32 arrayLayer) {
        ResourceID outputID;
        graph.AddPass("VisualizeBuffer")
            .Build([&](PassBuilder &builder) {
                builder.Read(sourceID, Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveImage(sourceID);
                desc.format = vk::Format::eR8G8B8A8Srgb;
                outputID = builder.OutputColorAttachment(0, "", desc, {LoadOp::DontCare, StoreOp::Store}).id;
            })
            .Execute([sourceID, arrayLayer](Resources &resources, CommandContext &cmd) {
                auto source = resources.GetImageView(sourceID);
                if (source->ArrayLayers() > 1 && arrayLayer != ~0u && arrayLayer < source->ArrayLayers()) {
                    source = resources.GetImageLayerView(sourceID, arrayLayer);
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
                    uint32 channel = CVarWindowViewChannel.Get();
                    switch (channel) {
                    case 3:
                        swizzle = 0b11111111; // aaaa
                        break;
                    case 2:
                        swizzle = 0b11101010; // bbba
                        break;
                    case 1:
                        swizzle = 0b11010101; // ggga
                        break;
                    case 0:
                    default:
                        swizzle = 0b11100100; // rgba
                        break;
                    }
                }
                cmd.SetShaderConstant(ShaderStage::Fragment, "SWIZZLE", swizzle);

                cmd.SetImageView("tex", source);
                cmd.Draw(3);
            });
        return outputID;
    }
} // namespace sp::vulkan::renderer
