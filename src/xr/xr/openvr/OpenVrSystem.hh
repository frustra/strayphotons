#pragma once

#include "ecs/NamedEntity.hh"
#include "ecs/components/XRView.hh"
#include "xr/XrSystem.hh"
// #include "xr/openvr/OpenVrAction.hh"

#include <openvr.h>

namespace vr {
    class IVRSystem;
}

namespace sp {
    class GraphicsContext;

    namespace xr {
        vr::EVREye MapXrEyeToOpenVr(ecs::XrEye eye);

        class OpenVrSystem final : public XrSystem {
        public:
            OpenVrSystem() {}

            void Init();
            bool IsInitialized();
            bool IsHmdPresent();

            // std::shared_ptr<XrActionSet> GetActionSet(std::string setName);

            bool GetPredictedViewPose(ecs::XrEye eye, glm::mat4 &invViewMat);
            // bool GetPredictedObjectPose(const TrackedObjectHandle &handle, glm::mat4 &objectPose);
            // std::vector<TrackedObjectHandle> GetTrackedObjectHandles();
            // std::shared_ptr<XrModel> GetTrackedObjectModel(const TrackedObjectHandle &handle);

            void SubmitView(ecs::XrEye eye, GraphicsContext *context, GpuTexture *tex);
            void WaitFrame();

        private:
            // vr::TrackedDeviceIndex_t GetOpenVrIndexFromHandle(const TrackedObjectHandle &handle);

            std::shared_ptr<vr::IVRSystem> vrSystem;
            // std::map<std::string, std::shared_ptr<OpenVrActionSet>> actionSets;

            ecs::NamedEntity vrOriginEntity;
            std::array<ecs::NamedEntity, (size_t)ecs::XrEye::EYE_COUNT> views = {
                ecs::NamedEntity("vr-eye-left"),
                ecs::NamedEntity("vr-eye-right"),
            };
        };

    } // namespace xr
} // namespace sp
