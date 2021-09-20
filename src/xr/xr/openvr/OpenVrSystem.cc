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
        std::string action_path = std::filesystem::absolute(cwd / "actions.json").string();
        vr::EVRInputError inputError = vr::VRInput()->SetActionManifestPath(action_path.c_str());
        Assert(inputError == vr::EVRInputError::VRInputError_None, "Failed to init SteamVR input");

        uint32_t vrWidth, vrHeight;
        vrSystem->GetRecommendedRenderTargetSize(&vrWidth, &vrHeight);
        Logf("OpenVR Render Target Size: %u x %u", vrWidth, vrHeight);

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            Tecs::Entity vrOrigin = vrOriginEntity.Get(lock);
            if (!vrOrigin) {
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

    /*std::shared_ptr<XrActionSet> OpenVrSystem::GetActionSet(std::string setName) {
        if (actionSets.count(setName) == 0) {
            actionSets[setName] = make_shared<OpenVrActionSet>(setName, "A SteamVr Action Set");
        }

        return actionSets[setName];
    }*/

    bool OpenVrSystem::GetPredictedViewPose(ecs::XrEye eye, glm::mat4 &invViewMat) {
        float secondsSinceLastVsync;
        vrSystem->GetTimeSinceLastVsync(&secondsSinceLastVsync, nullptr);

        float displayFrequency = vrSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
                                                                         vr::Prop_DisplayFrequency_Float);
        float vSyncToPhotons = vrSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
                                                                       vr::Prop_SecondsFromVsyncToPhotons_Float);

        vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd + 1];
        vrSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseOrigin::TrackingUniverseStanding,
                                                  (1.0f / displayFrequency) - secondsSinceLastVsync + vSyncToPhotons,
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

    /*bool OpenVrSystem::GetPredictedObjectPose(const TrackedObjectHandle &handle, glm::mat4 &objectPose) {
        // Work out which model to load
        vr::TrackedDeviceIndex_t deviceIndex = GetOpenVrIndexFromHandle(handle);

        static vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
        vr::EVRCompositorError error =
            vr::VRCompositor()->GetLastPoses(NULL, 0, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);

        if (error != vr::VRCompositorError_None) { return false; }

        if (!trackedDevicePoses[deviceIndex].bPoseIsValid) { return false; }

        glm::mat4 pos = glm::make_mat3x4((float *)trackedDevicePoses[deviceIndex].mDeviceToAbsoluteTracking.m);
        objectPose = pos;

        return true;
    }

    std::vector<TrackedObjectHandle> OpenVrSystem::GetTrackedObjectHandles() {
        // TODO: probably shouldn't run this logic on every frame
        std::vector<TrackedObjectHandle> connectedDevices = {
            {HMD, NONE, "xr-hmd", vrSystem->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd)},
        };

        return connectedDevices;
    }

    std::shared_ptr<XrModel> OpenVrSystem::GetTrackedObjectModel(const TrackedObjectHandle &handle) {
        return OpenVrModel::LoadOpenVrModel(GetOpenVrIndexFromHandle(handle));
    }

    vr::TrackedDeviceIndex_t OpenVrSystem::GetOpenVrIndexFromHandle(const TrackedObjectHandle &handle) {
        // Work out which model to load
        vr::TrackedDeviceIndex_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
        // vr::ETrackedDeviceClass desiredClass = vr::TrackedDeviceClass_Invalid;
        vr::ETrackedControllerRole desiredRole = vr::TrackedControllerRole_Invalid;

        if (handle.type == CONTROLLER) {
            // desiredClass = vr::TrackedDeviceClass_Controller;

            if (handle.hand == LEFT) {
                desiredRole = vr::TrackedControllerRole_LeftHand;
            } else if (handle.hand == RIGHT) {
                desiredRole = vr::TrackedControllerRole_RightHand;
            } else {
                Errorf("Loading models for ambidextrous controllers not supported");
                return vr::k_unTrackedDeviceIndexInvalid;
            }

            deviceIndex = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(desiredRole);
        } else if (handle.type == HMD) {
            deviceIndex = vr::k_unTrackedDeviceIndex_Hmd;
        } else {
            Errorf("Loading models for other types not yet supported");
            return vr::k_unTrackedDeviceIndexInvalid;
        }

        return deviceIndex;
    }*/

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
                case vr::VRState_Undefined:
                    str = "Undefined";
                    break;
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
                Debugf("[OVREvent] Property changed at %f: (%d) %d",
                       event.eventAgeSeconds,
                       event.trackedDeviceIndex,
                       event.data.property.prop);
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
                Debugf("[OVREvent] IPD changed at %f: %f", event.eventAgeSeconds, event.data.ipd.ipdMeters);
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
        for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
            if (!vrSystem->IsTrackedDeviceConnected(i)) continue;
            std::string deviceName = "";
            auto deviceClass = vrSystem->GetTrackedDeviceClass(i);
            switch (deviceClass) {
            case vr::TrackedDeviceClass_HMD:
                deviceName = "vr-hmd" + std::to_string(i);
                break;
            case vr::TrackedDeviceClass_Controller:
                deviceName = "vr-controller" + std::to_string(i);
                break;
            case vr::TrackedDeviceClass_GenericTracker:
                deviceName = "vr-tracker" + std::to_string(i);
                break;
            case vr::TrackedDeviceClass_TrackingReference:
            case vr::TrackedDeviceClass_DisplayRedirect:
            case vr::TrackedDeviceClass_Invalid:
                break;
            }
            if (trackedDevices[i].Name() != deviceName) {
                trackedDevices[i] = ecs::NamedEntity(deviceName);
                entitiesChanged = true;
            }
        }
        if (entitiesChanged) {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            Tecs::Entity vrOrigin = vrOriginEntity.Get(lock);
            Assert(vrOrigin, "VR Origin entity missing");

            for (auto &namedEntity : trackedDevices) {
                if (namedEntity.Name().empty()) continue;

                Tecs::Entity ent = namedEntity.Get(lock);
                if (!ent) {
                    ent = lock.NewEntity();
                    ent.Set<ecs::Name>(lock, namedEntity.Name());
                    ent.Set<ecs::Owner>(lock, ecs::Owner::SystemId::XR_MANAGER);
                    auto model = GAssets.LoadModel("box");
                    ent.Set<ecs::Renderable>(lock, model);
                    auto &transform = ent.Set<ecs::Transform>(lock);
                    transform.SetParent(vrOrigin);
                    transform.SetScale(glm::vec3(0.01f));

                    namedEntity = ecs::NamedEntity(namedEntity.Name(), ent);
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

                    transform.SetRotate(glm::mat4(glm::mat3(pose)));
                    transform.SetTranslate(glm::column(glm::mat4(), 3, glm::vec4(pose[3], 1.0f)));
                    transform.UpdateCachedTransform(lock);
                }
            }
        }
    }

} // namespace sp::xr
