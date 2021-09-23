#include "InputBindings.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "input/core/BindingNames.hh"
#include "xr/openvr/OpenVrSystem.hh"

#include <fstream>
#include <openvr.h>
#include <picojson/picojson.h>

namespace sp::xr {
    InputBindings::InputBindings(OpenVrSystem &vrSystem, std::string actionManifestPath) : vrSystem(vrSystem) {
        // TODO: Create .vrmanifest file / register vr manifest with steam:
        // https://github.com/ValveSoftware/openvr/wiki/Action-manifest
        vr::EVRInputError error = vr::VRInput()->SetActionManifestPath(actionManifestPath.c_str());
        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to initialize OpenVR input");

        std::string actionManifestStr;
        {
            std::ifstream file(actionManifestPath);
            Assert(file.is_open(), "Failed to open SteamVR action manifest file: " + actionManifestPath);

            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);

            actionManifestStr.resize(size);
            file.read(actionManifestStr.data(), size);
            file.close();
        }

        picojson::value root;
        string err = picojson::parse(root, actionManifestStr);
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
                        Assert(error == vr::EVRInputError::VRInputError_None,
                               "Failed to load OpenVR input action set: " + name);

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
                        Assert(error == vr::EVRInputError::VRInputError_None,
                               "Failed to load OpenVR input action set: " + action.name);
                    } else if (param.first == "type") {
                        auto typeStr = param.second.get<string>();
                        to_lower(typeStr);
                        action.type = Action::DataType::Invalid;
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
    }

    void InputBindings::Frame() {
        std::array<vr::VRInputValueHandle_t, vr::k_unMaxTrackedDeviceCount> origins;

        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::SignalOutput>>();

        for (auto &actionSet : actionSets) {
            vr::VRActiveActionSet_t activeActionSet = {};
            activeActionSet.ulActionSet = actionSet.handle;

            vr::EVRInputError error = vr::VRInput()->UpdateActionState(&activeActionSet,
                                                                       sizeof(vr::VRActiveActionSet_t),
                                                                       1);
            Assert(error == vr::EVRInputError::VRInputError_None,
                   "Failed to sync OpenVR actions for: " + actionSet.name);

            for (auto &action : actionSet.actions) {
                error =
                    vr::VRInput()->GetActionOrigins(actionSet.handle, action.handle, origins.data(), origins.size());
                Assert(error == vr::EVRInputError::VRInputError_None,
                       "Failed to read OpenVR action sources for: " + action.name);

                for (auto &originHandle : origins) {
                    if (originHandle) {
                        vr::InputOriginInfo_t originInfo;
                        error = vr::VRInput()->GetOriginTrackedDeviceInfo(originHandle,
                                                                          &originInfo,
                                                                          sizeof(originInfo));
                        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to read origin info");

                        Tecs::Entity originEntity = vrSystem.GetEntityForDeviceIndex(lock,
                                                                                     originInfo.trackedDeviceIndex);
                        if (originEntity && originEntity.Has<ecs::SignalOutput>(lock)) {
                            auto &signalOutput = originEntity.Get<ecs::SignalOutput>(lock);

                            vr::InputDigitalActionData_t digitalActionData;
                            vr::InputAnalogActionData_t analogActionData;
                            switch (action.type) {
                            case Action::DataType::Bool:
                                error = vr::VRInput()->GetDigitalActionData(action.handle,
                                                                            &digitalActionData,
                                                                            sizeof(vr::InputDigitalActionData_t),
                                                                            originInfo.devicePath);
                                Assert(error == vr::EVRInputError::VRInputError_None,
                                       "Failed to read OpenVR digital action: " + action.name);

                                if (digitalActionData.bActive) {
                                    signalOutput.SetSignal(action.name, digitalActionData.bState);
                                } else {
                                    signalOutput.ClearSignal(action.name);
                                }
                                break;
                            case Action::DataType::Vec1:
                            case Action::DataType::Vec2:
                            case Action::DataType::Vec3:
                                error = vr::VRInput()->GetAnalogActionData(action.handle,
                                                                           &analogActionData,
                                                                           sizeof(vr::InputAnalogActionData_t),
                                                                           originInfo.devicePath);
                                Assert(error == vr::EVRInputError::VRInputError_None,
                                       "Failed to read OpenVR analog action: " + action.name);

                                if (analogActionData.bActive) {
                                    switch (action.type) {
                                    case Action::DataType::Vec3:
                                        signalOutput.SetSignal(action.name + "_z", analogActionData.z);
                                    case Action::DataType::Vec2:
                                        signalOutput.SetSignal(action.name + "_y", analogActionData.y);
                                    case Action::DataType::Vec1:
                                        signalOutput.SetSignal(action.name + "_x", analogActionData.x);
                                        break;
                                    default:
                                        break;
                                    }
                                } else {
                                    signalOutput.ClearSignal(action.name + "_x");
                                    signalOutput.ClearSignal(action.name + "_y");
                                    signalOutput.ClearSignal(action.name + "_z");
                                }
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
} // namespace sp::xr
