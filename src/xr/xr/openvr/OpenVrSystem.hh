#pragma once

#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"
#include "xr/XrSystem.hh"
#include "xr/openvr/EventHandler.hh"
#include "xr/openvr/InputBindings.hh"

#include <atomic>
#include <memory>
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
            OpenVrSystem(GraphicsContext *context);
            ~OpenVrSystem();

            bool GetPredictedViewPose(ecs::XrEye eye, glm::mat4 &invViewMat) override;

            void SubmitView(ecs::XrEye eye, glm::mat4 &viewPose, GpuTexture *tex) override;
            void WaitFrame() override;

            ecs::NamedEntity GetEntityForDeviceIndex(size_t index);

            HiddenAreaMesh GetHiddenAreaMesh(ecs::XrEye eye) override;

        private:
            void Init() override;
            void Frame() override;

            void RegisterModels();

            GraphicsContext *context;

            std::atomic_flag loaded;
            std::shared_ptr<vr::IVRSystem> vrSystem;
            EventHandler eventHandler;
            std::shared_ptr<InputBindings> inputBindings;

            EnumArray<ecs::NamedEntity, ecs::XrEye> views = {
                {ecs::NamedEntity("vr-eye-left"), ecs::NamedEntity("vr-eye-right")},
            };

            ecs::NamedEntity vrOriginEntity = ecs::NamedEntity("player.vr-origin");
            ecs::NamedEntity vrHmdEntity = ecs::NamedEntity("player.vr-hmd");
            ecs::NamedEntity vrControllerLeftEntity = ecs::NamedEntity("player.vr-controller-left");
            ecs::NamedEntity vrControllerRightEntity = ecs::NamedEntity("player.vr-controller-right");
            std::array<ecs::NamedEntity, vr::k_unMaxTrackedDeviceCount> reservedEntities = {};
            std::array<ecs::NamedEntity *, vr::k_unMaxTrackedDeviceCount> trackedDevices = {};

            uint32 frameCountWorkaround = 0;
            int width = 0, height = 0;

            friend class EventHandler;
        };

    } // namespace xr
} // namespace sp
