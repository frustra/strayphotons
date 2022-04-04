#pragma once

#include <openvr.h>
#include <string>
#include <vector>

namespace sp::xr {
    class OpenVrSystem;

    class InputBindings {
    public:
        InputBindings(OpenVrSystem &vrSystem, std::string actionManifestPath);
        ~InputBindings();

        void Frame();

    private:
        OpenVrSystem &vrSystem;

        struct Action {
            enum class DataType {
                Bool,
                Vec1,
                Vec2,
                Vec3,
                Haptic,
                Pose,
                Skeleton,
                Count,
            };

            std::string name;
            vr::VRActionHandle_t handle;
            ecs::NamedEntity inputEntity;
            DataType type;

            Action() {}
            Action(std::string &name, vr::VRActionHandle_t handle, DataType type)
                : name(name), handle(handle), type(type) {}
        };

        struct ActionSet {
            std::string name;
            vr::VRActionSetHandle_t handle;
            std::vector<Action> actions;

            ActionSet() {}
            ActionSet(std::string &name, vr::VRActionSetHandle_t handle) : name(name), handle(handle) {}
        };

        std::vector<ActionSet> actionSets;
    };
} // namespace sp::xr
