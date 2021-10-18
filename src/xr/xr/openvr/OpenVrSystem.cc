// Project Headers
#include "OpenVrSystem.hh"

#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/core/GraphicsContext.hh"

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <openvr.h>
#include <stdexcept>
#include <thread>

namespace sp::xr {
    vr::EVREye MapXrEyeToOpenVr(ecs::XrEye eye) {
        switch (eye) {
        case ecs::XrEye::LEFT:
            return vr::Eye_Left;
        case ecs::XrEye::RIGHT:
            return vr::Eye_Right;
        default:
            Abort("Unknown XrEye enum: " + std::to_string((size_t)eye));
        }
    }

    OpenVrSystem::~OpenVrSystem() {
        StopThread();

        vrSystem.reset();
    }

    void OpenVrSystem::Init(GraphicsContext *context) {
        if (vrSystem) return;
        this->context = context;

        vr::EVRInitError err = vr::VRInitError_None;
        auto vrSystemPtr = vr::VR_Init(&err, vr::VRApplication_Scene);

        if (err == vr::VRInitError_None) {
            vrSystem = std::shared_ptr<vr::IVRSystem>(vrSystemPtr, [context](auto *ptr) {
                Logf("Shutting down OpenVR");
                context->WaitIdle();
                vr::VR_Shutdown();
            });
        } else {
            Errorf("Failed to load OpenVR system: %s", VR_GetVRInitErrorAsSymbol(err));
            Errorf("Run 'reloadxrsystem' in the console to try again.");
            return;
        }

        eventHandler = std::make_shared<EventHandler>(vrSystem);

        // Initialize SteamVR Input subsystem
        auto cwd = std::filesystem::current_path();
        std::string actionManifestPath = std::filesystem::absolute(cwd / "actions.json").string();
        inputBindings = std::make_shared<InputBindings>(*this, actionManifestPath);

        uint32_t vrWidth, vrHeight;
        vrSystem->GetRecommendedRenderTargetSize(&vrWidth, &vrHeight);
        Logf("OpenVR Render Target Size: %u x %u", vrWidth, vrHeight);

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            Tecs::Entity vrOrigin = vrOriginEntity.Get(lock);
            if (!vrOrigin) {
                Logf("Creating vr-origin");
                vrOrigin = lock.NewEntity();
                vrOrigin.Set<ecs::Name>(lock, "vr-origin");
                vrOrigin.Set<ecs::Owner>(lock, ecs::Owner::SystemId::XR_MANAGER);

                vrOriginEntity = ecs::NamedEntity("vr-origin", vrOrigin);
            }
            vrOrigin.Get<ecs::Transform>(lock);

            for (size_t i = 0; i < views.size(); i++) {
                auto &entity = views[i];

                Tecs::Entity ent = entity.Get(lock);
                if (!ent) {
                    ent = lock.NewEntity();
                    ent.Set<ecs::Name>(lock, entity.Name());
                    ent.Set<ecs::Owner>(lock, ecs::Owner::SystemId::XR_MANAGER);
                    ent.Set<ecs::XRView>(lock, (ecs::XrEye)i);
                    auto &transform = ent.Set<ecs::Transform>(lock);
                    transform.SetParent(vrOrigin);

                    entity = ecs::NamedEntity(entity.Name(), ent);
                }

                auto &view = ent.Set<ecs::View>(lock);
                view.extents = {vrWidth, vrHeight};
                view.clip = {0.1, 256};
                auto projMatrix = vrSystem->GetProjectionMatrix(MapXrEyeToOpenVr((ecs::XrEye)i),
                                                                view.clip.x,
                                                                view.clip.y);
                view.SetProjMat(glm::transpose(glm::make_mat4((float *)projMatrix.m)));
                view.visibilityMask.set(ecs::Renderable::VISIBLE_DIRECT_EYE);
            }
        }

        StartThread();
    }

    bool OpenVrSystem::IsInitialized() {
        return vrSystem != nullptr;
    }

    bool OpenVrSystem::IsHmdPresent() {
        return vr::VR_IsRuntimeInstalled() && vr::VR_IsHmdPresent();
    }

    bool OpenVrSystem::GetPredictedViewPose(ecs::XrEye eye, glm::mat4 &invViewMat) {
        float frameTimeRemaining = vr::VRCompositor()->GetFrameTimeRemaining();
        float vSyncToPhotons = vrSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
                                                                       vr::Prop_SecondsFromVsyncToPhotons_Float);

        vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd + 1];
        vrSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseOrigin::TrackingUniverseStanding,
                                                  frameTimeRemaining + vSyncToPhotons,
                                                  trackedDevicePoses,
                                                  vr::k_unTrackedDeviceIndex_Hmd + 1);

        if (trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {
            glm::mat4 hmdPose = glm::mat4(glm::make_mat3x4(
                (float *)trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m));

            vr::HmdMatrix34_t eyePosOvr = vrSystem->GetEyeToHeadTransform(MapXrEyeToOpenVr(eye));
            glm::mat4 eyeToHmdPose = glm::mat4(glm::make_mat3x4((float *)eyePosOvr.m));

            invViewMat = glm::transpose(eyeToHmdPose * hmdPose);

            return true;
        }

        return false;
    }

    void OpenVrSystem::WaitFrame() {
        vr::EVRCompositorError error = vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
        Assert(error == vr::EVRCompositorError::VRCompositorError_None,
               "WaitGetPoses failed: " + std::to_string((int)error));
    }

    ecs::NamedEntity OpenVrSystem::GetEntityForDeviceIndex(size_t index) {
        if (index >= trackedDevices.size()) return ecs::NamedEntity();

        return trackedDevices[index];
    }

    void OpenVrSystem::Frame() {
        if (eventHandler) eventHandler->Frame();

        vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
        vr::EVRCompositorError error =
            vr::VRCompositor()->GetLastPoses(trackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
        if (error != vr::VRCompositorError_None) return;

        bool entitiesChanged = false;
        std::array<std::string, vr::k_unMaxTrackedDeviceCount> deviceNames;
        for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
            if (vrSystem->IsTrackedDeviceConnected(i)) {
                auto deviceClass = vrSystem->GetTrackedDeviceClass(i);
                switch (deviceClass) {
                case vr::TrackedDeviceClass_HMD:
                    if (i == vr::k_unTrackedDeviceIndex_Hmd) {
                        deviceNames[i] = "vr-hmd";
                    } else {
                        deviceNames[i] = "vr-hmd" + std::to_string(i);
                    }
                    break;
                case vr::TrackedDeviceClass_Controller: {
                    auto role = vrSystem->GetControllerRoleForTrackedDeviceIndex(i);
                    if (role == vr::TrackedControllerRole_LeftHand) {
                        deviceNames[i] = "vr-controller-left";
                    } else if (role == vr::TrackedControllerRole_RightHand) {
                        deviceNames[i] = "vr-controller-right";
                    } else {
                        deviceNames[i] = "vr-controller" + std::to_string(i);
                    }
                } break;
                case vr::TrackedDeviceClass_GenericTracker:
                    deviceNames[i] = "vr-tracker" + std::to_string(i);
                    break;
                case vr::TrackedDeviceClass_TrackingReference:
                case vr::TrackedDeviceClass_DisplayRedirect:
                case vr::TrackedDeviceClass_Invalid:
                default:
                    break;
                }
            } else {
                deviceNames[i] = "";
            }
            if (trackedDevices[i].Name() != deviceNames[i]) entitiesChanged = true;
        }
        if (entitiesChanged) {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            Tecs::Entity vrOrigin = vrOriginEntity.Get(lock);
            if (vrOrigin) {
                for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                    auto &namedEntity = trackedDevices[i];
                    Tecs::Entity ent = namedEntity.Get(lock);

                    auto &targetName = deviceNames[i];
                    if (namedEntity.Name() != targetName) {
                        if (ent) {
                            ent.Destroy(lock);
                            ent = Tecs::Entity();
                        }
                        if (!targetName.empty()) {
                            ent = lock.NewEntity();
                            ent.Set<ecs::Name>(lock, targetName);
                            ent.Set<ecs::Owner>(lock, ecs::Owner::SystemId::XR_MANAGER);
                            auto model = GAssets.LoadModel("box");
                            ent.Set<ecs::Renderable>(lock, model);
                            auto &transform = ent.Set<ecs::Transform>(lock);
                            transform.SetParent(vrOrigin);
                            transform.SetScale(glm::vec3(0.01f));
                            ent.Set<ecs::EventBindings>(lock);
                            ent.Set<ecs::SignalOutput>(lock);
                        }
                        namedEntity = ecs::NamedEntity(targetName, ent);
                    }
                }
            }
        }
        {
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::Transform>>();

            Tecs::Entity vrOrigin = vrOriginEntity.Get(lock);
            if (vrOrigin) {
                for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                    auto &namedEntity = trackedDevices[i];

                    Tecs::Entity ent = namedEntity.Get(lock);
                    if (ent && ent.Has<ecs::Transform>(lock) && trackedDevicePoses[i].bPoseIsValid) {
                        auto &transform = ent.Get<ecs::Transform>(lock);

                        auto pose = glm::transpose(
                            glm::make_mat3x4((float *)trackedDevicePoses[i].mDeviceToAbsoluteTracking.m));

                        transform.SetRotation(glm::quat_cast(glm::mat3(pose)));
                        transform.SetPosition(pose[3]);
                        transform.UpdateCachedTransform(lock);
                    }
                }
            }
        }
        if (inputBindings) inputBindings->Frame();
    }

} // namespace sp::xr
