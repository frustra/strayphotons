// Project Headers
#include "OpenVrSystem.hh"

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
// #include "xr/openvr/OpenVrModel.hh"

// OpenVR headers
#include <openvr.h>

// GLM headers
#include <glm/glm.hpp>
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

    void OpenVrSystem::Init() {
        if (vrSystem) return;

        vr::EVRInitError err = vr::VRInitError_None;
        auto vrSystemPtr = vr::VR_Init(&err, vr::VRApplication_Scene);

        if (err == vr::VRInitError_None) {
            vrSystem = std::shared_ptr<vr::IVRSystem>(vrSystemPtr, [](auto *ptr) {
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
                    ent.Set<ecs::Transform>(lock);
                    ent.Set<ecs::XRView>(lock, (ecs::XrEye)i);

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
    }

    bool OpenVrSystem::IsInitialized() {
        return vrSystem != nullptr;
    }

    bool OpenVrSystem::IsHmdPresent() {
        return vr::VR_IsRuntimeInstalled() && vr::VR_IsHmdPresent();
    }

    void OpenVrSystem::WaitFrame() {
        // Throw away, we will use GetLastPoses() to access this in other places
        static vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
        vr::EVRCompositorError error =
            vr::VRCompositor()->WaitGetPoses(trackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);

        if (error != vr::EVRCompositorError::VRCompositorError_None) {
            // TODO: exception, or warning?
            Errorf("WaitGetPoses failed: %d", error);
        }
    }

    /*std::shared_ptr<XrActionSet> OpenVrSystem::GetActionSet(std::string setName) {
        if (actionSets.count(setName) == 0) {
            actionSets[setName] = make_shared<OpenVrActionSet>(setName, "A SteamVr Action Set");
        }

        return actionSets[setName];
    }*/

    bool OpenVrSystem::GetPredictedViewPose(ecs::XrEye eye, glm::mat4 &invViewMat) {
        float secondsSinceLastVsync;
        vrSystem->GetTimeSinceLastVsync(&secondsSinceLastVsync, NULL);

        float displayFrequency = vrSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
                                                                         vr::Prop_DisplayFrequency_Float);
        float vSyncToPhotons = vrSystem->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
                                                                       vr::Prop_SecondsFromVsyncToPhotons_Float);

        vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
        vrSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseOrigin::TrackingUniverseStanding,
                                                  (1.0f / displayFrequency) - secondsSinceLastVsync + vSyncToPhotons,
                                                  trackedDevicePoses,
                                                  vr::k_unMaxTrackedDeviceCount);

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

} // namespace sp::xr
