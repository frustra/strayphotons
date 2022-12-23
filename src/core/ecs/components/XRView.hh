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

    static StructMetadata MetadataXRView(typeid(XRView), StructField::New(&XRView::eye));
    static Component<XRView> ComponentXRView("xr_view", MetadataXRView);
}; // namespace ecs
