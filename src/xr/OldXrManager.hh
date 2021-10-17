#pragma once

#ifdef SP_XR_SUPPORT

    #include "core/console/CFunc.hh"
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

        private:
            Tecs::Entity ValidateAndLoadTrackedObject(sp::xr::TrackedObjectHandle &handle);
            Tecs::Entity UpdateXrActionEntity(xr::XrActionPtr action, bool active);

            void UpdateSkeletonDebugHand(xr::XrActionPtr action,
                                         glm::mat4 xrObjectPos,
                                         std::vector<xr::XrBoneData> &boneData,
                                         bool active);

            void ComputeBonePositions(std::vector<xr::XrBoneData> &boneData, std::vector<glm::mat4> &output);

            Tecs::Entity GetLaserPointer();

        private:
            Game *game;
            CFuncCollection funcs;

            std::shared_ptr<XrSystem> xrSystem;
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

#endif
