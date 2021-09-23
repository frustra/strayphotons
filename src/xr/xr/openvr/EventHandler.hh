#pragma once

#include <memory>

namespace vr {
    class IVRSystem;
}

namespace sp::xr {
    class OpenVrSystem;

    class EventHandler {
    public:
        EventHandler(std::shared_ptr<vr::IVRSystem> &vrSystem);

        void Frame();

    private:
        std::shared_ptr<vr::IVRSystem> vrSystem;
    };
} // namespace sp::xr
