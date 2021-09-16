#pragma once

#include "ecs/NamedEntity.hh"
#include "ecs/components/XRView.hh"
#include "xr/XrSystem.hh"
// #include "xr/openvr/OpenVrAction.hh"

namespace vr {
    class IVRSystem;
}

namespace sp {
    namespace xr {
        class OpenVrSystem final : public XrSystem {
        public:
            OpenVrSystem();

            void Init();
            bool IsInitialized();
            bool IsHmdPresent();

            // std::shared_ptr<XrActionSet> GetActionSet(std::string setName);

            // XrTracking functions
            // bool GetPredictedViewPose(ecs::XrEye eye, glm::mat4 &viewPose);
            // bool GetPredictedObjectPose(const TrackedObjectHandle &handle, glm::mat4 &objectPose);
            // std::vector<TrackedObjectHandle> GetTrackedObjectHandles();
            // std::shared_ptr<XrModel> GetTrackedObjectModel(const TrackedObjectHandle &handle);

            // XrCompositor functions
            RenderTarget *GetRenderTarget(ecs::XrEye eye);
            void SubmitView(ecs::XrEye eye, GpuTexture *tex);
            void WaitFrame();

        private:
            // vr::TrackedDeviceIndex_t GetOpenVrIndexFromHandle(const TrackedObjectHandle &handle);

            struct ViewInfo {
                ecs::XrEye eye;
                ecs::NamedEntity entity;
                std::shared_ptr<RenderTarget> renderTarget;

                ViewInfo() {}
                ViewInfo(ecs::XrEye eye) : eye(eye) {
                    if (eye == ecs::XrEye::LEFT) {
                        entity = ecs::NamedEntity("vr-eye-left");
                    } else if (eye == ecs::XrEye::RIGHT) {
                        entity = ecs::NamedEntity("vr-eye-right");
                    } else {
                        Abort("Unknown XrEye enum: " + std::to_string((size_t)eye));
                    }
                }
            };

            std::shared_ptr<vr::IVRSystem> vrSystem;
            // std::map<std::string, std::shared_ptr<OpenVrActionSet>> actionSets;

            ecs::NamedEntity vrOriginEntity;
            std::array<ViewInfo, (size_t)ecs::XrEye::EYE_COUNT> views;
        };

    } // namespace xr
} // namespace sp
