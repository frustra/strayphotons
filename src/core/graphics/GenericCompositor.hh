/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/InlineString.hh"
#include "ecs/StructMetadata.hh"
#include "gui/GuiDrawData.hh"

#include <cstdint>
#include <glm/glm.hpp>

namespace sp {
    class Image;
    class GpuTexture;
    class GuiContext;

    class GenericCompositor : public NonCopyable {
    public:
        virtual ~GenericCompositor() = default;

        typedef uint32_t ResourceID;
        typedef InlineString<127> ResourceName;

        static constexpr uint64_t FontAtlasID = (uint64_t)std::numeric_limits<ResourceID>::max() + 1;
        static constexpr ResourceID InvalidResource = ~0u;

        virtual void DrawGuiContext(GuiContext &context, glm::ivec4 viewport, glm::vec2 scale) = 0;
        virtual void DrawGuiData(const GuiDrawData &drawData, glm::ivec4 viewport, glm::vec2 scale) = 0;

        virtual std::shared_ptr<GpuTexture> UploadStaticImage(std::shared_ptr<const Image> image,
            bool genMipmap = true,
            bool srgb = true) = 0;
        virtual ResourceID AddStaticImage(const ResourceName &name, std::shared_ptr<GpuTexture> image) = 0;

        virtual void UpdateSourceImage(ecs::Entity dst, std::shared_ptr<sp::Image> src) = 0;
        void UpdateSourceImage(ecs::Entity dst,
            const uint8_t *data,
            size_t dataSize,
            uint32_t imageWidth,
            uint32_t imageHeight,
            uint32_t components);
    };

    static ecs::StructMetadata MetadataGuiDrawVertex(typeid(GuiDrawVertex),
        sizeof(GuiDrawVertex),
        "GuiDrawVertex",
        "A single 2D vertex with a position, color, and texture UVS",
        ecs::StructField::New("pos", "The position of this vertex", &GuiDrawVertex::pos),
        ecs::StructField::New("uv", "The UV texture coordinate to draw with", &GuiDrawVertex::uv),
        ecs::StructField::New("col", "The color of the vertex", &GuiDrawVertex::col));

    static ecs::StructMetadata MetadataGuiDrawCommand(typeid(GuiDrawCommand),
        sizeof(GuiDrawCommand),
        "GuiDrawCommand",
        "A single draw command that draws triangles within a clipping rectangle with a specific texture",
        ecs::StructField::New("clipRect", "A clipping rectangle to limit draw output", &GuiDrawCommand::clipRect),
        ecs::StructField::New("textureId", "The texture id to draw with", &GuiDrawCommand::textureId),
        ecs::StructField::New("indexCount",
            "The number of indices to draw (should be a multiple of 3)",
            &GuiDrawCommand::indexCount),
        ecs::StructField::New("vertexOffset",
            "An additional index offset to be applied to all vertices",
            &GuiDrawCommand::vertexOffset));

    static ecs::StructMetadata MetadataGuiDrawData(typeid(GuiDrawData),
        sizeof(GuiDrawData),
        "GuiDrawData",
        "A bundle of draw data used to describe rendering a GUI",
        ecs::StructField::New("drawCommands",
            "A list of draw commands that use the index and vertex buffers",
            &GuiDrawData::drawCommands),
        ecs::StructField::New("indexBuffer",
            "A list of indexes into the vertex buffer representing triangles",
            &GuiDrawData::indexBuffer),
        ecs::StructField::New("vertexBuffer",
            "A list of vertex points with color and texture UVs",
            &GuiDrawData::vertexBuffer));
} // namespace sp
