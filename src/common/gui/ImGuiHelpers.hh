/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Logging.hh"
#include "gui/GuiDrawData.hh"

#include <imgui.h>
#include <strayphotons/components.h>

namespace sp {
    static_assert(sizeof(ImDrawVert) == sizeof(GuiDrawVertex), "ImDrawVert size does not match");
    static_assert(offsetof(ImDrawVert, pos) == offsetof(GuiDrawVertex, pos), "ImDrawVert pos offset does not match");
    static_assert(offsetof(ImDrawVert, uv) == offsetof(GuiDrawVertex, uv), "ImDrawVert uv offset does not match");
    static_assert(offsetof(ImDrawVert, col) == offsetof(GuiDrawVertex, col), "ImDrawVert col offset does not match");

    static inline void ConvertImDrawData(ImDrawData *drawData, sp_gui_draw_data_t *output) {
        Assertf(drawData != nullptr, "GuiDrawData being constructed with null ImDrawData");

        size_t totalCommandCount = 0, totalIndexCount = 0, totalVertexCount = 0;
        for (const auto &cmdList : drawData->CmdLists) {
            totalCommandCount += cmdList->CmdBuffer.size();
            totalIndexCount += cmdList->IdxBuffer.size();
            totalVertexCount += cmdList->VtxBuffer.size();
        }
        sp_gui_draw_command_vector_resize(&output->drawCommands, totalCommandCount);
        sp_uint16_vector_resize(&output->indexBuffer, totalIndexCount);
        sp_gui_draw_vertex_vector_resize(&output->vertexBuffer, totalVertexCount);
        if (totalCommandCount == 0 || totalIndexCount == 0 || totalVertexCount == 0) return;

        auto *vertexIter = reinterpret_cast<GuiDrawVertex *>(sp_gui_draw_vertex_vector_get_data(&output->vertexBuffer));
        auto *indexIter = reinterpret_cast<GuiDrawIndex *>(sp_uint16_vector_get_data(&output->indexBuffer));
        for (const auto &cmdList : drawData->CmdLists) {
            GuiDrawVertex *begin = reinterpret_cast<GuiDrawVertex *>(cmdList->VtxBuffer.Data);
            std::copy(begin, begin + cmdList->VtxBuffer.size(), vertexIter);
            vertexIter += cmdList->VtxBuffer.size();
            std::copy(cmdList->IdxBuffer.begin(), cmdList->IdxBuffer.end(), indexIter);
            indexIter += cmdList->IdxBuffer.size();
        }

        auto *cmdIter = reinterpret_cast<GuiDrawCommand *>(sp_gui_draw_command_vector_get_data(&output->drawCommands));
        uint32_t vertexOffset = 0;
        for (const auto &cmdList : drawData->CmdLists) {
            for (const auto &cmd : cmdList->CmdBuffer) {
                Assertf(!cmd.UserCallback, "GuiDrawData UserCallback is not supported");

                cmdIter->clipRect = {cmd.ClipRect.x - drawData->DisplayPos.x,
                    cmd.ClipRect.y - drawData->DisplayPos.y,
                    cmd.ClipRect.z - drawData->DisplayPos.x,
                    cmd.ClipRect.w - drawData->DisplayPos.y};
                cmdIter->textureId = cmd.TextureId;
                cmdIter->indexCount = cmd.ElemCount;
                cmdIter->vertexOffset = vertexOffset + cmd.VtxOffset;
                cmdIter++;
            }
            vertexOffset += cmdList->VtxBuffer.size();
        }
    }
} // namespace sp
