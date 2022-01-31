// Project Headers
#include "OpenVrSystem.hh"

#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"
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
        case ecs::XrEye::Left:
            return vr::Eye_Left;
        case ecs::XrEye::Right:
            return vr::Eye_Right;
        default:
            Abortf("Unknown XrEye enum: %d", eye);
        }
    }

    OpenVrSystem::~OpenVrSystem() {
        StopThread();

        GetSceneManager().QueueActionAndBlock(SceneAction::RemoveScene, "vr-system");
        vrSystem.reset();
    }

    bool OpenVrSystem::Init(GraphicsContext *context) {
        if (vrSystem) return true;
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
            return false;
        }

        eventHandler = std::make_shared<EventHandler>(vrSystem);

        // Initialize SteamVR Input subsystem
        std::string actionManifestPath = std::filesystem::absolute("actions.json").string();
        inputBindings = std::make_shared<InputBindings>(*this, actionManifestPath);

        uint32_t vrWidth, vrHeight;
        vrSystem->GetRecommendedRenderTargetSize(&vrWidth, &vrHeight);
        Logf("OpenVR Render Target Size: %u x %u", vrWidth, vrHeight);

        RegisterModels();

        GetSceneManager().QueueActionAndBlock(SceneAction::AddSystemScene,
            "vr-system",
            [this, vrWidth, vrHeight](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto vrOrigin = lock.NewEntity();
                vrOrigin.Set<ecs::Name>(lock, vrOriginEntity.Name());
                vrOrigin.Set<ecs::SceneInfo>(lock, vrOrigin, ecs::SceneInfo::Priority::System, scene);
                vrOrigin.Set<ecs::Transform>(lock);

                static const std::array specialEntities = {vrHmdEntity,
                    vrControllerLeftEntity,
                    vrControllerRightEntity};
                for (auto &namedEntity : specialEntities) {
                    auto ent = lock.NewEntity();
                    ent.Set<ecs::Name>(lock, namedEntity.Name());
                    ent.Set<ecs::SceneInfo>(lock, ent, ecs::SceneInfo::Priority::System, scene);
                    ent.Set<ecs::Transform>(lock);
                    ent.Set<ecs::EventBindings>(lock);
                    ent.Set<ecs::SignalOutput>(lock);
                }

                for (size_t i = 0; i < reservedEntities.size(); i++) {
                    reservedEntities[i] = ecs::NamedEntity("vr-device" + std::to_string(i));
                }

                for (size_t i = 0; i < views.size(); i++) {
                    auto eye = (ecs::XrEye)i;

                    auto ent = lock.NewEntity();
                    ent.Set<ecs::Name>(lock, views[eye].Name());
                    ent.Set<ecs::SceneInfo>(lock, ent, ecs::SceneInfo::Priority::System, scene);
                    ent.Set<ecs::XRView>(lock, eye);

                    auto &transform = ent.Set<ecs::Transform>(lock);
                    transform.SetParent(vrOrigin);

                    auto &view = ent.Set<ecs::View>(lock);
                    view.extents = {vrWidth, vrHeight};
                    view.clip = {0.1, 256};
                    auto projMatrix = vrSystem->GetProjectionMatrix(MapXrEyeToOpenVr(eye), view.clip.x, view.clip.y);
                    view.SetProjMat(glm::transpose(glm::make_mat4((float *)projMatrix.m)));
                    view.visibilityMask.set(ecs::Renderable::VISIBLE_DIRECT_EYE);
                }
            });

        StartThread();
        return true;
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
        ZoneScoped;
        vr::EVRCompositorError error = vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
        Assert(error == vr::EVRCompositorError::VRCompositorError_None,
            "WaitGetPoses failed: " + std::to_string((int)error));
    }

    ecs::NamedEntity OpenVrSystem::GetEntityForDeviceIndex(size_t index) {
        if (index >= trackedDevices.size() || trackedDevices[index] == nullptr) return ecs::NamedEntity();

        return *trackedDevices[index];
    }

    void OpenVrSystem::Frame() {
        if (eventHandler) eventHandler->Frame();

        vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
        vr::EVRCompositorError error =
            vr::VRCompositor()->GetLastPoses(trackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
        if (error != vr::VRCompositorError_None) return;

        for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
            if (vrSystem->IsTrackedDeviceConnected(i)) {
                auto deviceClass = vrSystem->GetTrackedDeviceClass(i);
                if (deviceClass == vr::TrackedDeviceClass_HMD && i == vr::k_unTrackedDeviceIndex_Hmd) {
                    trackedDevices[i] = &vrHmdEntity;
                } else if (deviceClass == vr::TrackedDeviceClass_Controller) {
                    auto role = vrSystem->GetControllerRoleForTrackedDeviceIndex(i);
                    if (role == vr::TrackedControllerRole_LeftHand) {
                        trackedDevices[i] = &vrControllerLeftEntity;
                    } else if (role == vr::TrackedControllerRole_RightHand) {
                        trackedDevices[i] = &vrControllerRightEntity;
                    } else {
                        trackedDevices[i] = &reservedEntities[i];
                    }
                } else if (deviceClass == vr::TrackedDeviceClass_GenericTracker) {
                    trackedDevices[i] = &reservedEntities[i];
                } else {
                    // Ignore tracking references and other devices
                    trackedDevices[i] = nullptr;
                }
            } else {
                trackedDevices[i] = nullptr;
            }
        }
        bool missingEntities = false;
        {
            ZoneScopedN("Sync to ECS");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::Transform>>();

            for (auto namedEntity : trackedDevices) {
                if (namedEntity != nullptr && !namedEntity->Get(lock).Exists(lock)) {
                    missingEntities = true;
                    break;
                }
            }

            Tecs::Entity vrOrigin = vrOriginEntity.Get(lock);
            if (vrOrigin && vrOrigin.Has<ecs::Transform>(lock)) {
                for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                    if (trackedDevices[i] != nullptr) {
                        Tecs::Entity ent = trackedDevices[i]->Get(lock);
                        if (ent && ent.Has<ecs::Transform>(lock) && trackedDevicePoses[i].bPoseIsValid) {
                            auto &transform = ent.Get<ecs::Transform>(lock);

                            auto pose = glm::transpose(
                                glm::make_mat3x4((float *)trackedDevicePoses[i].mDeviceToAbsoluteTracking.m));

                            transform.SetParent(vrOrigin);
                            transform.SetMatrix(pose);
                        }
                    }
                }
            }
        }
        if (missingEntities) {
            GetSceneManager().QueueActionAndBlock(SceneAction::AddSystemScene,
                "vr-system",
                [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                    for (auto namedEntity : trackedDevices) {
                        if (namedEntity != nullptr && !namedEntity->Get(lock).Exists(lock)) {
                            auto ent = lock.NewEntity();
                            ent.Set<ecs::Name>(lock, namedEntity->Name());
                            ent.Set<ecs::SceneInfo>(lock, ent, ecs::SceneInfo::Priority::System, scene);
                            ent.Set<ecs::Transform>(lock);
                            ent.Set<ecs::EventBindings>(lock);
                            ent.Set<ecs::SignalOutput>(lock);
                        }
                    }
                });
        }
        if (inputBindings) inputBindings->Frame();
    }

    HiddenAreaMesh OpenVrSystem::GetHiddenAreaMesh(ecs::XrEye eye) {
        static_assert(sizeof(*vr::HiddenAreaMesh_t::pVertexData) == sizeof(*HiddenAreaMesh::vertices));
        auto mesh = vrSystem->GetHiddenAreaMesh(MapXrEyeToOpenVr(eye));
        return {(const glm::vec2 *)mesh.pVertexData, mesh.unTriangleCount};
    }

    void OpenVrSystem::RegisterModels() {
        auto modelPathLen =
            vr::VRResources()->GetResourceFullPath("vr_glove_left_model.glb", "rendermodels/vr_glove/", NULL, 0);
        std::vector<char> modelPathStr(modelPathLen);
        vr::VRResources()->GetResourceFullPath("vr_glove_left_model.glb",
            "rendermodels/vr_glove/",
            modelPathStr.data(),
            modelPathStr.size());
        GAssets.RegisterExternalModel("vr_glove_left", modelPathStr.data());

        modelPathLen =
            vr::VRResources()->GetResourceFullPath("vr_glove_right_model.glb", "rendermodels/vr_glove/", NULL, 0);
        modelPathStr.resize(modelPathLen);
        vr::VRResources()->GetResourceFullPath("vr_glove_right_model.glb",
            "rendermodels/vr_glove/",
            modelPathStr.data(),
            modelPathStr.size());
        GAssets.RegisterExternalModel("vr_glove_right", modelPathStr.data());
    }
} // namespace sp::xr
