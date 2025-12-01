/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "ecs/StructMetadata.hh"

#include <glm/glm.hpp>
#include <vector>

struct ImDrawVert;
struct ImDrawData;

namespace sp {
    struct GuiDrawVertex {
        glm::vec2 pos;
        glm::vec2 uv;
        uint32_t col;

        GuiDrawVertex() {}
        GuiDrawVertex(const ImDrawVert &vert);

        bool operator==(const GuiDrawVertex &) const = default;
    };

    static ecs::StructMetadata MetadataGuiDrawVertex(typeid(GuiDrawVertex),
        sizeof(GuiDrawVertex),
        "GuiDrawVertex",
        "A single 2D vertex with a position, color, and texture UVS",
        ecs::StructField::New("pos", "The position of this vertex", &GuiDrawVertex::pos),
        ecs::StructField::New("uv", "The UV texture coordinate to draw with", &GuiDrawVertex::uv),
        ecs::StructField::New("col", "The color of the vertex", &GuiDrawVertex::col));

    typedef uint16_t GuiDrawIndex;

    struct GuiDrawCommand {
        glm::vec4 clipRect;
        uint64_t textureId;
        uint32_t indexCount;
        uint32_t vertexOffset;

        bool operator==(const GuiDrawCommand &) const = default;
    };

    static ecs::StructMetadata MetadataGuiDrawCommand(typeid(GuiDrawCommand),
        sizeof(GuiDrawCommand),
        "GuiDrawCommand",
        "A single draw command that draws triangles within a clipping rectangle with a specific texture",
        ecs::StructField::New("clip_rect", "A clipping rectangle to limit draw output", &GuiDrawCommand::clipRect),
        ecs::StructField::New("texture_id", "The texture id to draw with", &GuiDrawCommand::textureId),
        ecs::StructField::New("index_count",
            "The number of indices to draw (should be a multiple of 3)",
            &GuiDrawCommand::indexCount));

    struct GuiDrawData {
        std::vector<GuiDrawCommand> drawCommands;
        std::vector<GuiDrawIndex> indexBuffer;
        std::vector<GuiDrawVertex> vertexBuffer;

        GuiDrawData() {}
        GuiDrawData(ImDrawData *imguiDrawData);
    };

    static ecs::StructMetadata MetadataGuiDrawData(typeid(GuiDrawData),
        sizeof(GuiDrawData),
        "GuiDrawData",
        "A bundle of draw data used to describe rendering a GUI",
        ecs::StructField::New("draw_commands",
            "A list of draw commands that use the index and vertex buffers",
            &GuiDrawData::drawCommands),
        ecs::StructField::New("index_buffer",
            "A list of indexes into the vertex buffer representing triangles",
            &GuiDrawData::indexBuffer),
        ecs::StructField::New("vertex_buffer",
            "A list of vertex points with color and texture UVs",
            &GuiDrawData::vertexBuffer));

    class GenericCompositor : public NonCopyable {
    public:
        virtual void DrawGui(const GuiDrawData &drawData, glm::ivec4 viewport, glm::vec2 scale) = 0;
    };
} // namespace sp
