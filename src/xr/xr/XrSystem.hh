#pragma once

#include "ecs/components/XRView.hh"

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>

namespace sp {
    class RenderTarget;
    class GpuTexture;
    class GraphicsContext;

    namespace xr {
        // What class of object is being tracked
        enum TrackedObjectType {
            HMD, // The tracked object represents the HMD pose
            CONTROLLER, // The tracked object represents a controller pose
            HAND, // The tracked object represents a hand pose
            OTHER, // The tracked object is some other entity
        };

        // What hand is the tracked object related to
        enum TrackedObjectHand {
            NONE, // For objects that cannot be related to a hand, like an HMD
            LEFT, // For objects that can only be held in a left hand, like a Touch controller
            RIGHT, // For objects that can only be held in a right hand, like a Touch controller
            BOTH, // For objects that are being held by both hands. Like a tracked gun
            EITHER, // For objects that can be held by either hand. Like a Vive Wand.
        };

        struct TrackedObjectHandle {
            TrackedObjectType type;
            TrackedObjectHand hand;
            std::string name;
            bool connected;
        };

        struct HiddenAreaMesh {
            const glm::vec2 *vertices = nullptr;
            uint32_t triangleCount = 0;
        };

        class XrSystem {
        public:
            virtual bool GetPredictedViewPose(ecs::XrEye eye, glm::mat4 &invViewMat) = 0;

            virtual void SubmitView(ecs::XrEye eye, glm::mat4 &viewPose, GpuTexture *tex) = 0;
            virtual void WaitFrame() = 0;

            virtual HiddenAreaMesh GetHiddenAreaMesh(ecs::XrEye eye) = 0;
        };

    } // namespace xr
} // namespace sp
