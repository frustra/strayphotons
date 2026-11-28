/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "VisualizeBuffer.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "vulkan/vulkan.hpp"

#include <cctype>
#include <cstdint>

namespace sp::vulkan::renderer {
    static CVar<std::string> CVarWindowViewChannels("r.WindowViewChannels",
        "RGB",
        "The set of color channel to views. (up to 4 channels mapped to RGBA)");

    ResourceID VisualizeBuffer(RenderGraph &graph, ResourceID sourceID, uint32_t arrayLayer) {
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
                } else if (source->MipLevels() > 1 && arrayLayer < source->MipLevels()) {
                    source = resources.GetImageMipView(sourceID, arrayLayer);
                }

                if (source->Format() == vk::Format::eR32Uint) {
                    Assertf(source->ViewType() != vk::ImageViewType::e2DArray,
                        "Visualizing r32 image array unsupported: %s",
                        resources.GetName(sourceID));
                    cmd.SetShaders("screen_cover.vert", "visualize_buffer_uimage2D.frag");
                } else if (source->ViewType() == vk::ImageViewType::e2DArray) {
                    cmd.SetShaders("screen_cover.vert", "visualize_buffer_sampler2DArray.frag");

                    struct {
                        float layer = 0;
                    } push;
                    cmd.PushConstants(push);
                } else {
                    cmd.SetShaders("screen_cover.vert", "visualize_buffer_sampler2D.frag");
                }

                auto format = source->Format();
                uint32_t components = FormatComponentCount(format);
                std::string swizzleStr = CVarWindowViewChannels.Get();
                uint32_t swizzleMask = 0;
                for (uint32_t c = 0; c < 4; c++) {
                    char ch = c < swizzleStr.size() ? std::toupper(swizzleStr[c]) : ' ';
                    if (ch == 'R' && components >= 1) {
                        swizzleMask |= (1 << (8 + c)); // | (0b00 << (2 * c));
                    } else if (ch == 'G' && components >= 2) {
                        swizzleMask |= (1 << (8 + c)) | (0b01 << (2 * c));
                    } else if (ch == 'B' && components >= 3) {
                        swizzleMask |= (1 << (8 + c)) | (0b10 << (2 * c));
                    } else if (ch == 'A' && components >= 4) {
                        swizzleMask |= (1 << (8 + c)) | (0b11 << (2 * c));
                    } else {
                        swizzleMask |= c << (2 * c);
                    }
                }
                cmd.SetShaderConstant(ShaderStage::Fragment, "SWIZZLE_MASK", swizzleMask);

                cmd.SetImageView("tex", source);
                cmd.Draw(3);
            });
        return outputID;
    }
} // namespace sp::vulkan::renderer
