#include "InputBindings.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"
#include "xr/openvr/OpenVrSystem.hh"

#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <openvr.h>
#include <picojson/picojson.h>

namespace sp::xr {
    InputBindings::InputBindings(OpenVrSystem &vrSystem, std::string actionManifestPath) : vrSystem(vrSystem) {
        // TODO: Create .vrmanifest file / register vr manifest with steam:
        // https://github.com/ValveSoftware/openvr/wiki/Action-manifest
        vr::EVRInputError error = vr::VRInput()->SetActionManifestPath(actionManifestPath.c_str());
        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to initialize OpenVR input");

        auto actionManifest = GAssets.Load(actionManifestPath, AssetType::External, true)->Get();
        Assertf(actionManifest, "Failed to load vr action manifest: %s", actionManifestPath);

        picojson::value root;
        string err = picojson::parse(root, actionManifest->String());
        if (!err.empty()) {
            Errorf("Failed to parse OpenVR action manifest file: %s", err);
            return;
        }

        auto actionSetList = root.get<picojson::object>()["action_sets"];
        if (actionSetList.is<picojson::array>()) {
            for (auto actionSet : actionSetList.get<picojson::array>()) {
                for (auto param : actionSet.get<picojson::object>()) {
                    if (param.first == "name") {
                        auto name = param.second.get<string>();
                        vr::VRActionSetHandle_t actionSetHandle;
                        error = vr::VRInput()->GetActionSetHandle(name.c_str(), &actionSetHandle);
                        Assertf(error == vr::EVRInputError::VRInputError_None,
                            "Failed to load OpenVR input action set: %s",
                            name);

                        actionSets.emplace_back(name, actionSetHandle);
                    }
                }
            }
        }

        auto actionList = root.get<picojson::object>()["actions"];
        if (actionList.is<picojson::array>()) {
            for (auto actionObj : actionList.get<picojson::array>()) {
                Action action;
                for (auto param : actionObj.get<picojson::object>()) {
                    if (param.first == "name") {
                        action.name = param.second.get<string>();
                        error = vr::VRInput()->GetActionHandle(action.name.c_str(), &action.handle);
                        Assertf(error == vr::EVRInputError::VRInputError_None,
                            "Failed to load OpenVR input action set: %s",
                            action.name);
                    } else if (param.first == "type") {
                        auto typeStr = param.second.get<string>();
                        to_lower(typeStr);
                        action.type = Action::DataType::Count;
                        if (typeStr == "boolean") {
                            action.type = Action::DataType::Bool;
                        } else if (typeStr == "vector1") {
                            action.type = Action::DataType::Vec1;
                        } else if (typeStr == "vector2") {
                            action.type = Action::DataType::Vec2;
                        } else if (typeStr == "vector3") {
                            action.type = Action::DataType::Vec3;
                        } else if (typeStr == "vibration") {
                            action.type = Action::DataType::Haptic;
                        } else if (typeStr == "pose") {
                            action.type = Action::DataType::Pose;
                        } else if (typeStr == "skeleton") {
                            action.type = Action::DataType::Skeleton;
                        } else {
                            Errorf("OpenVR action manifest contains unknown action type: %s", typeStr);
                        }
                    }
                }
                if (!action.name.empty() && action.handle) {
                    auto it = std::find_if(actionSets.begin(), actionSets.end(), [&action](auto &set) {
                        return starts_with(action.name, set.name);
                    });
                    if (it != actionSets.end()) {
                        it->actions.push_back(action);
                    } else {
                        Logf("OpenVR Action has unknown set: %s", action.name);
                    }
                }
            }
        }

        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "vr_input",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                for (auto &actionSet : actionSets) {
                    for (auto &action : actionSet.actions) {
                        if (action.type == Action::DataType::Pose || action.type == Action::DataType::Skeleton) {
                            auto inputName = "vr" + action.name;
                            std::transform(inputName.begin(), inputName.end(), inputName.begin(), [](unsigned char c) {
                                return (c == ':' || c == '/') ? '_' : c;
                            });
                            action.poseEntity = ecs::NamedEntity("input", inputName);
                            auto ent = scene->NewSystemEntity(lock, scene, action.poseEntity.Name());
                            ent.Set<ecs::TransformTree>(lock);
                            ent.Set<ecs::SignalOutput>(lock);
                            // ent.Set<ecs::Renderable>(lock, GAssets.LoadGltf("box"));
                        }
                    }
                }
            });
    }

    InputBindings::~InputBindings() {
        GetSceneManager().QueueActionAndBlock(SceneAction::RemoveScene, "vr_input");
    }

    void InputBindings::Frame() {
        std::array<vr::VRInputValueHandle_t, vr::k_unMaxTrackedDeviceCount> origins;

        bool missingEntities = false;
        {
            ZoneScopedN("InputBindings Sync to ECS");
            auto lock =
                ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::FocusLayer, ecs::FocusLock, ecs::EventBindings>,
                    ecs::Write<ecs::EventInput, ecs::SignalOutput, ecs::TransformTree>>();

            for (auto &actionSet : actionSets) {
                vr::VRActiveActionSet_t activeActionSet = {};
                activeActionSet.ulActionSet = actionSet.handle;

                vr::EVRInputError error = vr::VRInput()->UpdateActionState(&activeActionSet,
                    sizeof(vr::VRActiveActionSet_t),
                    1);
                Assertf(error == vr::EVRInputError::VRInputError_None,
                    "Failed to sync OpenVR actions for: %s",
                    actionSet.name);

                for (auto &action : actionSet.actions) {
                    error = vr::VRInput()->GetActionOrigins(actionSet.handle,
                        action.handle,
                        origins.data(),
                        origins.size());
                    Assertf(error == vr::EVRInputError::VRInputError_None,
                        "Failed to read OpenVR action sources for: %s",
                        action.name);

                    for (auto &originHandle : origins) {
                        if (!originHandle) continue;
                        vr::InputOriginInfo_t originInfo;
                        error = vr::VRInput()->GetOriginTrackedDeviceInfo(originHandle,
                            &originInfo,
                            sizeof(originInfo));
                        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to read origin info");

                        std::string actionSignal = action.name;
                        if (actionSignal.size() > 0 && actionSignal.at(0) == '/') {
                            actionSignal = actionSignal.substr(1);
                        }

                        ecs::NamedEntity originEntity = vrSystem.GetEntityForDeviceIndex(originInfo.trackedDeviceIndex);
                        ecs::Entity entity = originEntity.Get(lock);
                        if (!entity) continue;

                        vr::InputDigitalActionData_t digitalActionData;
                        vr::InputAnalogActionData_t analogActionData;
                        vr::InputPoseActionData_t poseActionData;
                        vr::InputSkeletalActionData_t skeletalActionData;
                        switch (action.type) {
                        case Action::DataType::Bool:
                            error = vr::VRInput()->GetDigitalActionData(action.handle,
                                &digitalActionData,
                                sizeof(vr::InputDigitalActionData_t),
                                originInfo.devicePath);
                            Assertf(error == vr::EVRInputError::VRInputError_None,
                                "Failed to read OpenVR digital action: %s",
                                action.name);

                            if (entity.Has<ecs::EventBindings>(lock)) {
                                auto &bindings = entity.Get<ecs::EventBindings>(lock);

                                if (digitalActionData.bActive && digitalActionData.bChanged) {
                                    bindings.SendEvent(lock, action.name, originEntity, digitalActionData.bState);
                                }
                            }

                            if (entity.Has<ecs::SignalOutput>(lock)) {
                                auto &signalOutput = entity.Get<ecs::SignalOutput>(lock);

                                if (digitalActionData.bActive) {
                                    signalOutput.SetSignal(actionSignal, digitalActionData.bState);
                                } else {
                                    signalOutput.ClearSignal(actionSignal);
                                }
                            }
                            break;
                        case Action::DataType::Vec1:
                        case Action::DataType::Vec2:
                        case Action::DataType::Vec3:
                            error = vr::VRInput()->GetAnalogActionData(action.handle,
                                &analogActionData,
                                sizeof(vr::InputAnalogActionData_t),
                                originInfo.devicePath);
                            Assertf(error == vr::EVRInputError::VRInputError_None,
                                "Failed to read OpenVR analog action: %s",
                                action.name);

                            if (entity.Has<ecs::EventBindings>(lock)) {
                                auto &bindings = entity.Get<ecs::EventBindings>(lock);

                                if (analogActionData.bActive &&
                                    (analogActionData.x != 0.0f || analogActionData.y != 0.0f ||
                                        analogActionData.z != 0.0f)) {
                                    switch (action.type) {
                                    case Action::DataType::Vec1:
                                        bindings.SendEvent(lock, action.name, originEntity, analogActionData.x);
                                        break;
                                    case Action::DataType::Vec2:
                                        bindings.SendEvent(lock,
                                            action.name,
                                            originEntity,
                                            glm::vec2(analogActionData.x, analogActionData.y));
                                        break;
                                    case Action::DataType::Vec3:
                                        bindings.SendEvent(lock,
                                            action.name,
                                            originEntity,
                                            glm::vec3(analogActionData.x, analogActionData.y, analogActionData.z));
                                        break;
                                    default:
                                        break;
                                    }
                                }
                            }

                            if (entity.Has<ecs::SignalOutput>(lock)) {
                                auto &signalOutput = entity.Get<ecs::SignalOutput>(lock);

                                if (analogActionData.bActive) {
                                    switch (action.type) {
                                    case Action::DataType::Vec3:
                                        signalOutput.SetSignal(actionSignal + "_z", analogActionData.z);
                                    case Action::DataType::Vec2:
                                        signalOutput.SetSignal(actionSignal + "_y", analogActionData.y);
                                    case Action::DataType::Vec1:
                                        signalOutput.SetSignal(actionSignal + "_x", analogActionData.x);
                                        break;
                                    default:
                                        break;
                                    }
                                } else {
                                    signalOutput.ClearSignal(actionSignal + "_x");
                                    signalOutput.ClearSignal(actionSignal + "_y");
                                    signalOutput.ClearSignal(actionSignal + "_z");
                                }
                            }
                            break;
                        case Action::DataType::Pose:
                            error = vr::VRInput()->GetPoseActionDataForNextFrame(action.handle,
                                vr::ETrackingUniverseOrigin::TrackingUniverseStanding,
                                &poseActionData,
                                sizeof(vr::InputPoseActionData_t),
                                originInfo.devicePath);
                            Assertf(error == vr::EVRInputError::VRInputError_None,
                                "Failed to read OpenVR pose action: %s",
                                action.name);

                            if (poseActionData.bActive) {
                                if (poseActionData.pose.bDeviceIsConnected && poseActionData.pose.bPoseIsValid) {
                                    ecs::Entity vrOrigin = vrSystem.vrOriginEntity.Get(lock);
                                    ecs::Entity poseEntity = action.poseEntity.Get(lock);
                                    if (poseEntity.Has<ecs::TransformTree>(lock)) {
                                        auto &transform = poseEntity.Get<ecs::TransformTree>(lock);

                                        auto &poseMat = poseActionData.pose.mDeviceToAbsoluteTracking.m;
                                        transform.pose = glm::transpose(glm::make_mat3x4((float *)poseMat));
                                        transform.parent = vrOrigin;
                                    }
                                }
                            }
                            break;
                        case Action::DataType::Skeleton:
                            error = vr::VRInput()->GetSkeletalActionData(action.handle,
                                &skeletalActionData,
                                sizeof(vr::InputSkeletalActionData_t));
                            Assertf(error == vr::EVRInputError::VRInputError_None,
                                "Failed to read OpenVR skeleton action: %s",
                                action.name);

                            if (skeletalActionData.bActive) {
                                error = vr::VRInput()->GetPoseActionDataForNextFrame(action.handle,
                                    vr::ETrackingUniverseOrigin::TrackingUniverseStanding,
                                    &poseActionData,
                                    sizeof(vr::InputPoseActionData_t),
                                    vr::k_ulInvalidInputValueHandle);
                                Assertf(error == vr::EVRInputError::VRInputError_None,
                                    "Failed to read OpenVR pose action: %s",
                                    action.name);

                                if (poseActionData.bActive && poseActionData.pose.bDeviceIsConnected &&
                                    poseActionData.pose.bPoseIsValid) {
                                    ecs::Entity vrOrigin = vrSystem.vrOriginEntity.Get(lock);
                                    ecs::Entity poseEntity = action.poseEntity.Get(lock);
                                    if (poseEntity.Has<ecs::TransformTree>(lock)) {
                                        auto &transform = poseEntity.Get<ecs::TransformTree>(lock);

                                        auto &poseMat = poseActionData.pose.mDeviceToAbsoluteTracking.m;
                                        transform.pose = glm::transpose(glm::make_mat3x4((float *)poseMat));
                                        transform.parent = vrOrigin;
                                    }

                                    uint32_t boneCount = 0;
                                    error = vr::VRInput()->GetBoneCount(action.handle, &boneCount);
                                    if (error != vr::EVRInputError::VRInputError_None) {
                                        Errorf("Failed to get bone count for action skeleton: %s", action.name);
                                        continue;
                                    }

                                    std::vector<vr::VRBoneTransform_t> boneTransforms(boneCount);
                                    // error = vr::VRInput()->GetSkeletalReferenceTransforms(action.handle,
                                    //     vr::VRSkeletalTransformSpace_Model,
                                    //     vr::VRSkeletalReferencePose_OpenHand,
                                    //     boneTransforms.data(),
                                    //     boneTransforms.size());
                                    error = vr::VRInput()->GetSkeletalBoneData(action.handle,
                                        vr::VRSkeletalTransformSpace_Model,
                                        vr::VRSkeletalMotionRange_WithoutController,
                                        boneTransforms.data(),
                                        boneTransforms.size());
                                    Assertf(error == vr::EVRInputError::VRInputError_None,
                                        "Failed to read OpenVR bone transforms for action: %s",
                                        action.name);

                                    action.boneEntities.resize(boneCount);

                                    for (size_t i = 0; i < boneCount; i++) {
                                        std::array<char, vr::k_unMaxBoneNameLength> boneNameChars;
                                        error = vr::VRInput()->GetBoneName(action.handle,
                                            i,
                                            boneNameChars.data(),
                                            boneNameChars.size());
                                        Assertf(error == vr::EVRInputError::VRInputError_None,
                                            "Failed to read OpenVR bone name %d for action: %s",
                                            i,
                                            action.name);
                                        std::string boneName(boneNameChars.begin(),
                                            std::find(boneNameChars.begin(), boneNameChars.end(), '\0'));
                                        auto entityName = action.poseEntity.Name();
                                        entityName.entity += "." + boneName;

                                        if (action.boneEntities[i].Name() != entityName) {
                                            action.boneEntities[i] = ecs::NamedEntity(entityName);
                                            missingEntities = true;
                                            continue;
                                        }

                                        ecs::Entity boneEntity = action.boneEntities[i].Get(lock);
                                        if (boneEntity.Has<ecs::TransformTree>(lock)) {
                                            auto &transform = boneEntity.Get<ecs::TransformTree>(lock);

                                            transform.pose.SetRotation(glm::quat(boneTransforms[i].orientation.w,
                                                boneTransforms[i].orientation.x,
                                                boneTransforms[i].orientation.y,
                                                boneTransforms[i].orientation.z));
                                            transform.pose.SetPosition(glm::make_vec3(boneTransforms[i].position.v));
                                            transform.parent = poseEntity;

                                            // temporary hack to pose the hands
                                            if (action.poseEntity.Name().entity == "vr_actions_main_in_lefthand_anim") {
                                                entityName.Parse("vr:left_hand." + boneName);
                                            } else if (action.poseEntity.Name().entity ==
                                                       "vr_actions_main_in_righthand_anim") {
                                                entityName.Parse("vr:right_hand." + boneName);
                                            } else {
                                                continue;
                                            }

                                            ecs::NamedEntity target(entityName); // TODO: replace with EntityRef
                                            auto targetEntity = target.Get(lock);
                                            if (targetEntity && targetEntity.Has<ecs::TransformTree>(lock)) {
                                                auto &targetTransform = targetEntity.Get<ecs::TransformTree>(lock);
                                                targetTransform.parent = boneEntity;
                                                targetTransform.pose = {};
                                            }
                                        }
                                    }
                                }
                            }
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }

        if (missingEntities) {
            ZoneScopedN("InputBindings::AddMissingEntities");
            GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
                "vr_input",
                [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                    for (auto &actionSet : actionSets) {
                        for (auto &action : actionSet.actions) {
                            if (action.type == Action::DataType::Skeleton) {
                                for (auto &boneEnt : action.boneEntities) {
                                    auto ent = scene->NewSystemEntity(lock, scene, boneEnt.Name());
                                    ent.Set<ecs::TransformTree>(lock);
                                    ent.Set<ecs::SignalOutput>(lock);
                                }
                            }
                        }
                    }
                });
        }
    }
} // namespace sp::xr
