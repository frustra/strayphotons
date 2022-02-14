#pragma once

#include <memory>

namespace vr {
    class IVRSystem;
}

namespace sp::xr {
    class OpenVrSystem;

    class EventHandler {
    public:
        EventHandler(const OpenVrSystem &vrSystem);

        void Frame();

    private:
        const OpenVrSystem &vrSystem;
    };
} // namespace sp::xr
