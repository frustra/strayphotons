/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GenericCompositor.hh"

#include "assets/Image.hh"

namespace sp {

    void GenericCompositor::UpdateSourceImage(ecs::Entity dst,
        const uint8_t *data,
        size_t dataSize,
        uint32_t imageWidth,
        uint32_t imageHeight,
        uint32_t components) {
        UpdateSourceImage(dst, std::make_shared<Image>(data, dataSize, imageWidth, imageHeight, components));
    }

} // namespace sp
