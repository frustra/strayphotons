#pragma once

#include <ecs/Components.hh>

namespace ecs {
    enum class XrEye : size_t {
        LEFT = 0,
        RIGHT,
        EYE_COUNT,
    };

    struct XRView {
        XrEye eye;

        XRView() : eye(XrEye::EYE_COUNT) {}
        XRView(XrEye eye) : eye(eye) {}
    };

    static Component<XRView> ComponentXRView("xrview");
}; // namespace ecs
