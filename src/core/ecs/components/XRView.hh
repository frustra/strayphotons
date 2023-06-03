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

    static StructMetadata MetadataXRView(typeid(XRView), "xr_view", StructField::New(&XRView::eye));
    static Component<XRView> ComponentXRView(MetadataXRView);
}; // namespace ecs
