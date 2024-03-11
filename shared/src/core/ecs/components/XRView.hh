/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <ecs/Components.hh>

namespace ecs {
    enum class XrEye {
        Left = 0,
        Right,
    };

    struct XRView {
        XrEye eye = XrEye::Left;

        XRView() {}
        XRView(XrEye eye) : eye(eye) {}
    };

    static StructMetadata MetadataXRView(typeid(XRView), "xr_view", "", StructField::New(&XRView::eye));
    static Component<XRView> ComponentXRView(MetadataXRView);
}; // namespace ecs
