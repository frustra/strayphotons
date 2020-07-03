#pragma once

#include "xr/XrModel.hh"
#include <glm/glm.hpp> // For glm::mat4

namespace sp
{
namespace xr
{
    // What class of object is being tracked
    enum TrackedObjectType
    {
        HMD,            // The tracked object represents the HMD pose
        CONTROLLER,     // The tracked object represents a controller pose
        HAND,           // The tracked object represents a hand pose
        OTHER,          // The tracked object is some other entity
    };

    // What hand is the tracked object related to
    enum TrackedObjectHand
    {
        NONE,       // For objects that cannot be related to a hand, like an HMD
        LEFT,       // For objects that can only be held in a left hand, like a Touch controller
        RIGHT,      // For objects that can only be held in a right hand, like a Touch controller
        BOTH,       // For objects that are being held by both hands. Like a tracked gun
        EITHER,     // For objects that can be held by either hand. Like a Vive Wand.
    };

    struct TrackedObjectHandle
    {
        TrackedObjectType type;
        TrackedObjectHand hand;
        std::string name;
        bool connected;
    };

    // This class contains tracking functionality for the HMD, eye position, and any arbitrary "tracked objects" exposed by the XR Runtime.
    // Conceptually, this means it must:
    //  - Provide methods to get the pose of a View at the predicted render time
    //  - Provide methods to get a tracked object pose at the predicted render time (for render loop)
    //  - Provide methods to get a tracked object pose at the current time (for game loop / physics loop)

    class XrTracking
    {
    public:
        // Get a list of generic "objects" which can be tracked by this XR Runtime.
        // NOTE: these are generally not XR Controllers or XR Skeletons: those are tracked by
        // the XrAction module. Instead, these are generic objects that should be tracked and optionally
        // rendered in the world. Currently, this is used for the HMD
        virtual std::vector<TrackedObjectHandle> GetTrackedObjectHandles() = 0;

        // Create an XrModel for a tracked object (if the runtime has a supported model for the tracked object).
        // NOTE: This function ALWAYS creates a new model
        virtual std::shared_ptr<XrModel> GetTrackedObjectModel(const TrackedObjectHandle &handle) = 0;

        // Provides the predicted pose of a View during the current frame.
        // Must be called after XrCompositor::WaitFrame() or you will end up
        // with predicted HMD poses from the previous frame.
        virtual bool GetPredictedViewPose(size_t view, glm::mat4 &viewPose) = 0;

        // Provides the predicted pose of a tracked object during the _NEXT_ frame.
        // This is used for gameplay. The sequence is:
        // 1. XrCompositor::WaitFrame() (computes tracking data for views at Frame N, controllers at frame N+1)
        // 2. Render Scene
        // 3. XrCompositor::EndFrame()
        // 4. Move controller model to position from 1.
        // 5. GOTO 1
        virtual bool GetPredictedObjectPose(const TrackedObjectHandle &handle, glm::mat4 &objectPose) = 0;
    };
}
}
