#pragma once

#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
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

            bool Initialized() override;
            bool GetPredictedViewPose(ecs::XrEye eye, glm::mat4 &invViewMat) override;

            void SubmitView(ecs::XrEye eye, glm::mat4 &viewPose, GpuTexture *tex) override;
            void WaitFrame() override;

            HiddenAreaMesh GetHiddenAreaMesh(ecs::XrEye eye) override;

        private:
            bool ThreadInit() override;
            void Frame() override;

            void RegisterModels();
            ecs::Entity GetEntityForDeviceIndex(size_t index);

            GraphicsContext *context;

            std::atomic_flag loaded;
            std::shared_ptr<vr::IVRSystem> vrSystem;
            EventHandler eventHandler;
            std::shared_ptr<InputBindings> inputBindings;

            EnumArray<ecs::EntityRef, ecs::XrEye> views;

            ecs::EntityRef vrOriginEntity, vrHmdEntity;
            ecs::EntityRef vrControllerLeftEntity, vrControllerRightEntity;
            std::array<ecs::EntityRef, vr::k_unMaxTrackedDeviceCount> reservedEntities = {};
            std::array<ecs::EntityRef *, vr::k_unMaxTrackedDeviceCount> trackedDevices = {};

            uint32 frameCountWorkaround = 0;
            int texWidth = 0, texHeight = 0;

            friend class EventHandler;
            friend class InputBindings;
        };

    } // namespace xr
} // namespace sp
