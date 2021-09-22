#pragma once

#include <openvr.h>

namespace sp::xr {
    // Must match the OpenVR manifest files!
    static const char *const GameActionSet = "/actions/main";

    static const char *const GrabActionName = "/actions/main/in/grab";
    static const char *const TeleportActionName = "/actions/main/in/teleport";
    static const char *const MovementActionName = "/actions/main/in/movement";
    static const char *const LeftHandActionName = "/actions/main/in/LeftHand";
    static const char *const RightHandActionName = "/actions/main/in/RightHand";

    static const char *const LeftHandSkeletonActionName = "/actions/main/in/lefthand_anim";
    static const char *const RightHandSkeletonActionName = "/actions/main/in/righthand_anim";

    static const char *const SubpathLeftHand = "/user/hand/left";
    static const char *const SubpathRightHand = "/user/hand/right";
    static const char *const SubpathUser = "/user";
    static const char *const SubpathNone = "";

    class OpenVrSystem;

    class InputBindings {
    public:
        InputBindings(OpenVrSystem &vrSystem, std::string actionManifestPath);

        void Frame();

    private:
        OpenVrSystem &vrSystem;

        vr::VRActionSetHandle_t mainActionSet;
        vr::VRActionHandle_t grabAction, teleportAction, movementAction;
    };
} // namespace sp::xr
