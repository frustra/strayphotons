/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "strayphotons/cpp/HeapVector.hh"

#include <glm/glm.hpp>

namespace sp {
    struct GuiDrawVertex {
        glm::vec2 pos;
        glm::vec2 uv;
        uint32_t col;

        bool operator==(const GuiDrawVertex &) const = default;
    };

    typedef uint16_t GuiDrawIndex;

    struct GuiDrawCommand {
        glm::vec4 clipRect;
        uint64_t textureId;
        uint32_t indexCount;
        uint32_t vertexOffset;

        bool operator==(const GuiDrawCommand &) const = default;
    };

    struct GuiDrawData {
        HeapVector<GuiDrawCommand> drawCommands;
        HeapVector<GuiDrawIndex> indexBuffer;
        HeapVector<GuiDrawVertex> vertexBuffer;
    };
} // namespace sp
