// Project Headers
#include "OpenVrSystem.hh"

#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/core/GraphicsContext.hh"

// OpenVR headers
#include <openvr.h>

// GLM headers
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/type_ptr.hpp>

// System Headers
#include <filesystem>
#include <memory>
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
            return vr::Eye_Left;
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
            Abort(VR_GetVRInitErrorAsSymbol(err));
        }

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
                vrOrigin.Set<ecs::Transform>(lock);

                vrOriginEntity = ecs::NamedEntity("vr-origin", vrOrigin);
            }

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
                view.projMat = glm::transpose(glm::make_mat4((float *)projMatrix.m));
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

    void OpenVrSystem::WaitFrame() {
        vr::EVRCompositorError error = vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
        Assert(error == vr::EVRCompositorError::VRCompositorError_None,
               "WaitGetPoses failed: " + std::to_string((int)error));
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

    Tecs::Entity OpenVrSystem::GetEntityForDeviceIndex(ecs::Lock<ecs::Read<ecs::Name>> lock, size_t index) {
        if (index >= trackedDevices.size()) return Tecs::Entity();

        return trackedDevices[index].Get(lock);
    }

    void OpenVrSystem::Frame() {
        vr::VREvent_t event;
        while (vrSystem && vrSystem->PollNextEvent(&event, sizeof(event))) {
            std::string str;

            switch (event.eventType) {
            case vr::VREvent_TrackedDeviceActivated:
                Debugf("[OVREvent] Controller activated at %f: %d", event.eventAgeSeconds, event.trackedDeviceIndex);
                break;
            case vr::VREvent_TrackedDeviceDeactivated:
                Debugf("[OVREvent] Controller deactivated at %f: %d", event.eventAgeSeconds, event.trackedDeviceIndex);
                break;
            case vr::VREvent_TrackedDeviceUpdated:
                Debugf("[OVREvent] Controller updated at %f: %d", event.eventAgeSeconds, event.trackedDeviceIndex);
                break;
            case vr::VREvent_InputFocusCaptured:
            case vr::VREvent_InputFocusChanged:
            case vr::VREvent_InputFocusReleased:
                Debugf("[OVREvent] Input focus changed at %f: (%d) %d -> %d",
                       event.eventAgeSeconds,
                       event.eventType,
                       event.data.process.oldPid,
                       event.data.process.pid);
                break;
            case vr::VREvent_SceneApplicationChanged:
                Debugf("[OVREvent] Scene application changed at %f: %d -> %d",
                       event.eventAgeSeconds,
                       event.data.process.oldPid,
                       event.data.process.pid);
                break;
            case vr::VREvent_SceneApplicationStateChanged:
                switch (vr::VRApplications()->GetSceneApplicationState()) {
                case vr::EVRSceneApplicationState_None:
                    str = "None";
                    break;
                case vr::EVRSceneApplicationState_Starting:
                    str = "Starting";
                    break;
                case vr::EVRSceneApplicationState_Quitting:
                    str = "Quitting";
                    break;
                case vr::EVRSceneApplicationState_Running:
                    str = "Running";
                    break;
                case vr::EVRSceneApplicationState_Waiting:
                    str = "Waiting";
                    break;
                }
                Debugf("[OVREvent] Scene application state changed at %f: %s", event.eventAgeSeconds, str);
                break;
            case vr::VREvent_Compositor_ApplicationResumed:
                Debugf("[OVREvent] Application resumed at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_StatusUpdate:
                switch (event.data.status.statusState) {
                case vr::VRState_Off:
                    str = "Off";
                    break;
                case vr::VRState_Searching:
                    str = "Searching";
                    break;
                case vr::VRState_Searching_Alert:
                    str = "Searching_Alert";
                    break;
                case vr::VRState_Ready:
                    str = "Ready";
                    break;
                case vr::VRState_Ready_Alert:
                    str = "Ready_Alert";
                    break;
                case vr::VRState_NotReady:
                    str = "NotReady";
                    break;
                case vr::VRState_Standby:
                    str = "Standby";
                    break;
                case vr::VRState_Ready_Alert_Low:
                    str = "Ready_Alert_Low";
                    break;
                default:
                    str = "Undefined";
                    break;
                }
                Debugf("[OVREvent] Status updated at %f: %s", event.eventAgeSeconds, str);
                break;
            case vr::VREvent_EnterStandbyMode:
                Debugf("[OVREvent] Entered standby mode at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_LeaveStandbyMode:
                Debugf("[OVREvent] Left standby mode at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_DashboardActivated:
                Debugf("[OVREvent] Dashboard activated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_DashboardDeactivated:
                Debugf("[OVREvent] Dashboard deactivated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_TrackedDeviceUserInteractionStarted:
                Debugf("[OVREvent] User interaction started at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_TrackedDeviceUserInteractionEnded:
                Debugf("[OVREvent] User interaction ended at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_TrackedDeviceRoleChanged: {
                Debugf("[OVREvent] Device role changed at %f: (%d) %s",
                       event.eventAgeSeconds,
                       event.trackedDeviceIndex,
                       str);
                vr::ETrackedControllerRole role = vr::TrackedControllerRole_Max;
                for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                    role = vrSystem->GetControllerRoleForTrackedDeviceIndex(i);
                    switch (role) {
                    case vr::TrackedControllerRole_Invalid:
                        str = "";
                        break;
                    case vr::TrackedControllerRole_LeftHand:
                        str = "LeftHand";
                        break;
                    case vr::TrackedControllerRole_RightHand:
                        str = "RightHand";
                        break;
                    case vr::TrackedControllerRole_OptOut:
                        str = "OptOut";
                        break;
                    case vr::TrackedControllerRole_Treadmill:
                        str = "Treadmill";
                        break;
                    case vr::TrackedControllerRole_Stylus:
                        str = "Stylus";
                        break;
                    }
                    if (!str.empty()) { Debugf("[OVREvent]   Device role for (%u): %s", i, str); }
                }
            } break;
            case vr::VREvent_PropertyChanged:
                // Debugf("[OVREvent] Property changed at %f: (%d) %d",
                //        event.eventAgeSeconds,
                //        event.trackedDeviceIndex,
                //        event.data.property.prop);
                break;
            case vr::VREvent_ChaperoneFlushCache:
                Debugf("[OVREvent] Chaperone cache refresh at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ChaperoneUniverseHasChanged:
                Debugf("[OVREvent] Chaperone updated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Compositor_ChaperoneBoundsShown:
                Debugf("[OVREvent] Chaperone bounds shown at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Compositor_ChaperoneBoundsHidden:
                Debugf("[OVREvent] Chaperone bounds hidden at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_IpdChanged:
                // Debugf("[OVREvent] IPD changed at %f: %f", event.eventAgeSeconds, event.data.ipd.ipdMeters);
                break;
            case vr::VREvent_OtherSectionSettingChanged:
                Debugf("[OVREvent] Settings: Other section changed at %f: %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ApplicationListUpdated:
                Debugf("[OVREvent] Applications list updated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Input_ActionManifestReloaded:
                Debugf("[OVREvent] Action manifest reloaded at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ActionBindingReloaded:
                Debugf("[OVREvent] Action bindings reloaded at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Input_BindingLoadFailed:
                Errorf("[OVREvent] Binding load failed at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Input_BindingLoadSuccessful:
                Debugf("[OVREvent] Binding load succeeded at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Input_BindingsUpdated:
                Debugf("[OVREvent] Bindings updated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_DesktopViewUpdating:
                Debugf("[OVREvent] Desktop view updating at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_DesktopViewReady:
                Debugf("[OVREvent] Desktop view ready at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ProcessConnected:
                Debugf("[OVREvent] Process connected at %f: %u", event.eventAgeSeconds, event.data.process.pid);
                break;
            case vr::VREvent_ProcessDisconnected:
                Debugf("[OVREvent] Process disconnected at %f: %u", event.eventAgeSeconds, event.data.process.pid);
                break;
            case vr::VREvent_Quit:
                Logf("[OVREvent] OpenVR quiting at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ProcessQuit:
                Debugf("[OVREvent] OpenVR process quitting at %f: %u", event.eventAgeSeconds, event.data.process.pid);
                break;
            case vr::VREvent_Compositor_ApplicationNotResponding:
                Logf("[OVREvent] OpenVR Compositor not responding at %f", event.eventAgeSeconds);
                break;
            default:
                Debugf("[OVREvent] Unknown OpenVR event: %d", event.eventType);
            }
        }

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
                    deviceNames[i] = "vr-hmd" + std::to_string(i);
                    break;
                case vr::TrackedDeviceClass_Controller:
                    deviceNames[i] = "vr-controller" + std::to_string(i);
                    break;
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
            Assert(vrOrigin, "VR Origin entity missing");

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
        {
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::Transform>>();

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
        if (inputBindings) inputBindings->Frame();
    }

} // namespace sp::xr
