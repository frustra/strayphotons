#pragma once

#include <ecs/Components.hh>

namespace ecs {
    enum class XrEye : size_t {
        Left = 0,
        Right,
        Count,
    };

    struct XRView {
        XrEye eye;

        XRView() : eye(XrEye::Count) {}
        XRView(XrEye eye) : eye(eye) {}
    };

    static Component<XRView> ComponentXRView("xrview");
}; // namespace ecs
