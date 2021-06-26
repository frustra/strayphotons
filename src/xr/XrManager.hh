#pragma once

#include "core/CFunc.hh"
#include "ecs/Ecs.hh"
#include "xr/XrSystem.hh"

namespace sp {
    class Game;

    namespace xr {
        class XrManager {
        public:
            XrManager(Game *game);
            ~XrManager();

            bool Frame(double dtSinceLastFrame);

            xr::XrSystemPtr GetXrSystem();

        private:
            void InitXrActions();

            ecs::Entity ValidateAndLoadTrackedObject(sp::xr::TrackedObjectHandle &handle);
            ecs::Entity UpdateXrActionEntity(xr::XrActionPtr action, bool active);

            void UpdateSkeletonDebugHand(xr::XrActionPtr action,
                                         glm::mat4 xrObjectPos,
                                         std::vector<xr::XrBoneData> &boneData,
                                         bool active);

            void ComputeBonePositions(std::vector<xr::XrBoneData> &boneData, std::vector<glm::mat4> &output);

            ecs::Entity GetLaserPointer();
            void SetVrOrigin();

            ecs::Entity CreateXrEntity();

            void LoadXrSystem();

        private:
            Game *game;
            CFuncCollection funcs;

            xr::XrSystemPtr xrSystem;
            xr::XrActionSetPtr gameActionSet;

            // Actions we use in game navigation
            xr::XrActionPtr teleportAction;
            xr::XrActionPtr grabAction;

            // Actions for the raw input device pose
            xr::XrActionPtr leftHandAction;
            xr::XrActionPtr rightHandAction;

            // Actions for the skeleton pose
            xr::XrActionPtr leftHandSkeletonAction;
            xr::XrActionPtr rightHandSkeletonAction;
        };
    } // namespace xr
} // namespace sp
