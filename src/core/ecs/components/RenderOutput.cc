/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "RenderOutput.hh"

namespace ecs {
    template<>
    void EntityComponent<RenderOutput>::Apply(RenderOutput &dst, const RenderOutput &src, bool liveTarget) {
        if (dst.guiElements.empty()) {
            dst.guiElements = src.guiElements;
        } else if (!src.guiElements.empty()) {
            for (auto &srcEnt : src.guiElements) {
                if (!sp::contains(dst.guiElements, srcEnt)) {
                    dst.guiElements.emplace_back(srcEnt);
                }
            }
        }
    }
} // namespace ecs
