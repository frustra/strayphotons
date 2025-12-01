/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "GenericCompositor.hh"

#include <imgui.h>

namespace sp {
    GuiDrawVertex::GuiDrawVertex(const ImDrawVert &vert) : pos(vert.pos), uv(vert.uv), col(vert.col) {}

    GuiDrawData::GuiDrawData(ImDrawData *drawData) {
        ZoneScoped;
        Assertf(drawData != nullptr, "GuiDrawData being constructed with null ImDrawData");

        size_t totalCommandCount = 0, totalIndexCount = 0, totalVertexCount = 0;
        for (const auto &cmdList : drawData->CmdLists) {
            totalCommandCount += cmdList->CmdBuffer.size();
            totalIndexCount += cmdList->IdxBuffer.size();
            totalVertexCount += cmdList->VtxBuffer.size();
        }
        if (totalCommandCount == 0 || totalIndexCount == 0 || totalVertexCount == 0) return;

        drawCommands.reserve(totalCommandCount);
        indexBuffer.resize(totalIndexCount);
        vertexBuffer.resize(totalVertexCount);

        auto vertexIter = vertexBuffer.begin();
        auto indexIter = indexBuffer.begin();
        for (const auto &cmdList : drawData->CmdLists) {
            std::copy(cmdList->VtxBuffer.begin(), cmdList->VtxBuffer.end(), vertexIter);
            vertexIter += cmdList->VtxBuffer.size();
            std::copy(cmdList->IdxBuffer.begin(), cmdList->IdxBuffer.end(), indexIter);
            indexIter += cmdList->IdxBuffer.size();
        }

        uint32_t vertexOffset = 0;
        for (const auto &cmdList : drawData->CmdLists) {
            for (const auto &cmd : cmdList->CmdBuffer) {
                Assertf(!cmd.UserCallback, "GuiDrawData UserCallback is not supported");

                drawCommands.emplace_back(GuiDrawCommand{glm::vec4(cmd.ClipRect.x - drawData->DisplayPos.x,
                                                             cmd.ClipRect.y - drawData->DisplayPos.y,
                                                             cmd.ClipRect.z - drawData->DisplayPos.x,
                                                             cmd.ClipRect.w - drawData->DisplayPos.y),
                    cmd.TextureId,
                    cmd.ElemCount,
                    vertexOffset});
            }
            vertexOffset += cmdList->VtxBuffer.size();
        }
    }
} // namespace sp
