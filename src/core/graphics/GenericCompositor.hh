/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"

// TODO: Define a StrayPhotons-native version of ImDrawData
struct ImDrawData;

namespace sp {
    class GenericCompositor : public NonCopyable {
    public:
        virtual void DrawGui(ImDrawData *drawData, glm::ivec4 viewport, glm::vec2 scale) = 0;
    };
} // namespace sp
