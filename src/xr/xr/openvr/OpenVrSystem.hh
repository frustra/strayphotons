#pragma once

#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"
#include "ecs/components/XRView.hh"
#include "xr/XrSystem.hh"
#include "xr/openvr/EventHandler.hh"
#include "xr/openvr/InputBindings.hh"

#include <openvr.h>

namespace vr {
    class IVRSystem;
}

namespace sp {
    class GraphicsContext;

    namespace xr {
        vr::EVREye MapXrEyeToOpenVr(ecs::XrEye eye);

        class OpenVrSystem final : public XrSystem, RegisteredThread {
        public:
            OpenVrSystem() : RegisteredThread("OpenVR", 120.0) {}
            ~OpenVrSystem();

            void Init(GraphicsContext *context);
            bool IsInitialized();
            bool IsHmdPresent();

            bool GetPredictedViewPose(ecs::XrEye eye, glm::mat4 &invViewMat);

            void SubmitView(ecs::XrEye eye, glm::mat4 &viewPose, GpuTexture *tex);
            void WaitFrame();

            Tecs::Entity GetEntityForDeviceIndex(ecs::Lock<ecs::Read<ecs::Name>> lock, size_t index);

        private:
            void Frame() override;

            GraphicsContext *context = nullptr;

            std::shared_ptr<vr::IVRSystem> vrSystem;
            std::shared_ptr<EventHandler> eventHandler;
            std::shared_ptr<InputBindings> inputBindings;

            ecs::NamedEntity vrOriginEntity = ecs::NamedEntity("vr-origin");
            std::array<ecs::NamedEntity, (size_t)ecs::XrEye::EYE_COUNT> views = {
                ecs::NamedEntity("vr-eye-left"),
                ecs::NamedEntity("vr-eye-right"),
            };

            std::array<ecs::NamedEntity, vr::k_unMaxTrackedDeviceCount> trackedDevices = {};

            uint32 frameCountWorkaround = 0;
            GpuTexture *rightEyeTexture = nullptr;
        };

    } // namespace xr
} // namespace sp
