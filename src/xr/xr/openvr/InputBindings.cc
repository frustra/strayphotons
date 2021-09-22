#include "InputBindings.hh"

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "input/core/BindingNames.hh"
#include "xr/openvr/OpenVrSystem.hh"

#include <openvr.h>

namespace sp::xr {
    InputBindings::InputBindings(OpenVrSystem &vrSystem, std::string actionManifestPath) : vrSystem(vrSystem) {
        // TODO: Create .vrmanifest file / register vr manifest with steam:
        // https://github.com/ValveSoftware/openvr/wiki/Action-manifest
        vr::EVRInputError error = vr::VRInput()->SetActionManifestPath(actionManifestPath.c_str());
        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to initialize OpenVR input");

        error = vr::VRInput()->GetActionSetHandle(GameActionSet, &mainActionSet);
        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to load OpenVR input action set");

        error = vr::VRInput()->GetActionHandle(GrabActionName, &grabAction);
        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to load OpenVR Grab action");

        error = vr::VRInput()->GetActionHandle(TeleportActionName, &teleportAction);
        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to load OpenVR Teleport action");

        error = vr::VRInput()->GetActionHandle(MovementActionName, &movementAction);
        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to load OpenVR Movement action");
    }

    void InputBindings::Frame() {
        vr::VRActiveActionSet_t activeActionSet = {};
        activeActionSet.ulActionSet = mainActionSet;

        vr::EVRInputError error = vr::VRInput()->UpdateActionState(&activeActionSet,
                                                                   sizeof(vr::VRActiveActionSet_t),
                                                                   1);
        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to sync OpenVR actions");

        vr::InputAnalogActionData_t movementActionData;
        error = vr::VRInput()->GetAnalogActionData(movementAction,
                                                   &movementActionData,
                                                   sizeof(vr::InputAnalogActionData_t),
                                                   0);
        Assert(error == vr::EVRInputError::VRInputError_None, "Failed to read OpenVR Movement action");

        {
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::SignalOutput>>();

            Tecs::Entity originEntity = vrSystem.GetEntityForDeviceIndex(lock, vr::k_unTrackedDeviceIndex_Hmd);
            if (originEntity && originEntity.Has<ecs::SignalOutput>(lock)) {
                auto &signalOutput = originEntity.Get<ecs::SignalOutput>(lock);
                if (movementActionData.bActive) {
                    signalOutput.SetSignal(INPUT_SIGNAL_MOVE_RIGHT, movementActionData.x);
                    signalOutput.SetSignal(INPUT_SIGNAL_MOVE_FORWARD, movementActionData.y);
                } else {
                    signalOutput.ClearSignal(INPUT_SIGNAL_MOVE_RIGHT);
                    signalOutput.ClearSignal(INPUT_SIGNAL_MOVE_FORWARD);
                }
            }
        }
    }
} // namespace sp::xr
