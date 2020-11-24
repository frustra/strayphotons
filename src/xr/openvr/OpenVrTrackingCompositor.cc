#include "xr/openvr/OpenVrTrackingCompositor.hh"

#include "ecs/components/View.hh"
#include "graphics/RenderTarget.hh"
#include "xr/openvr/OpenVrModel.hh"
#include "xr/openvr/OpenVrSystem.hh"

#include <core/Logging.hh>
#include <memory>
#include <stdexcept>

// GLM headers
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace sp;
using namespace xr;

OpenVrTrackingCompositor::OpenVrTrackingCompositor(vr::IVRSystem *vrs) : vrSystem(vrs) {
    if (vrs == nullptr) { throw std::runtime_error("Cannot initialize OpenVrTrackingCompositor with NULL IVRSystem"); }

    uint32_t vrWidth, vrHeight;
    GetRecommendedRenderTargetSize(vrWidth, vrHeight);

    Logf("OpenVr Render Target Size: %d x %d", vrWidth, vrHeight);

    // Reserve enough space in the vector for all our views
    viewRenderTargets.resize(GetNumViews());

    for (size_t i = 0; i < GetNumViews(); i++) {
        const RenderTargetDesc desc = RenderTargetDesc(PF_SRGB8_A8, glm::ivec2(vrWidth, vrHeight));

        // Create a render target for this XR View
        viewRenderTargets[i] = std::make_shared<RenderTarget>(RenderTarget(desc));

        CreateRenderTargetTexture(viewRenderTargets[i], desc);
    }
}

bool OpenVrTrackingCompositor::GetPredictedViewPose(size_t view, glm::mat4 &viewPose) {
    static vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    vr::EVRCompositorError error =
        vr::VRCompositor()->GetLastPoses(trackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    if (error != vr::EVRCompositorError::VRCompositorError_None) {
        Errorf("Failed to get view pose");
        return false;
    }

    if (trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {
        glm::mat4 hmdPose = glm::mat4(
            glm::make_mat3x4((float *)trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m));

        vr::EVREye eyeType = (view == 0) ? vr::Eye_Left : vr::Eye_Right;
        vr::HmdMatrix34_t eyePosOvr = vrSystem->GetEyeToHeadTransform(eyeType);

        glm::mat4 eyeToHmdPose = glm::mat4(glm::make_mat3x4((float *)eyePosOvr.m));

        viewPose = eyeToHmdPose * hmdPose;

        return true;
    }

    return false;
}

bool OpenVrTrackingCompositor::GetPredictedObjectPose(const TrackedObjectHandle &handle, glm::mat4 &objectPose) {
    // Work out which model to load
    vr::TrackedDeviceIndex_t deviceIndex = GetOpenVrIndexFromHandle(handle);

    static vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    vr::EVRCompositorError error =
        vr::VRCompositor()->GetLastPoses(NULL, 0, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);

    if (error != vr::VRCompositorError_None) { return false; }

    if (!trackedDevicePoses[deviceIndex].bPoseIsValid) { return false; }

    glm::mat4 pos = glm::make_mat3x4((float *)trackedDevicePoses[deviceIndex].mDeviceToAbsoluteTracking.m);
    objectPose = pos;

    return true;
}

RenderTarget::Ref OpenVrTrackingCompositor::GetRenderTarget(size_t view) {
    return viewRenderTargets[view];
}

void OpenVrTrackingCompositor::PopulateView(size_t view, ecs::Handle<ecs::View> &ecsView) {
    uint32_t vrWidth, vrHeight;
    GetRecommendedRenderTargetSize(vrWidth, vrHeight);

    ecsView->extents = {vrWidth, vrHeight};
    ecsView->clip = {0.1, 256};
    ecsView->projMat = glm::transpose(GetViewProjectionMatrix(view, ecsView->clip.x, ecsView->clip.y));
    ecsView->viewType = ecs::View::VIEW_TYPE_XR;
}

void OpenVrTrackingCompositor::SubmitView(size_t view, RenderTarget::Ref rt) {
    // TODO: helper function that verifies "eye" is a sane value and returns vr::Eye_XXXX based on the value.
    vr::EVREye eyeType = (view == 0) ? vr::Eye_Left : vr::Eye_Right;

    // TODO: use XrCompositor::Submit(), don't do this in the render function
    vr::Texture_t vrTexture = {(void *)rt->GetTexture().handle, vr::TextureType_OpenGL, vr::ColorSpace_Linear};
    vr::VRCompositor()->Submit(eyeType, &vrTexture);
}

void OpenVrTrackingCompositor::WaitFrame() {
    // Throw away, we will use GetLastPoses() to access this in other places
    static vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    vr::EVRCompositorError error =
        vr::VRCompositor()->WaitGetPoses(trackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    if (error != vr::EVRCompositorError::VRCompositorError_None) {
        // TODO: exception, or warning?
        Errorf("WaitGetPoses failed: %d", error);
    }
}

void OpenVrTrackingCompositor::BeginFrame() {
    // Not required for OpenVR
}

void OpenVrTrackingCompositor::EndFrame() {
    // Not required for OpenVR

    // TODO: could use this to indicate that tracked controller data is now valid?
    // TODO: Comments in OpenVr.h suggest a glFlush() here might be useful
    // TODO: Investigate vr::System::PostPresentHandoff()
}

size_t OpenVrTrackingCompositor::GetNumViews(bool minimum) {
    // OpenVR only supports 2 Eyes for Views.
    return 2;
}

void OpenVrTrackingCompositor::GetRecommendedRenderTargetSize(uint32_t &width, uint32_t &height) {
    vrSystem->GetRecommendedRenderTargetSize(&width, &height);
}

glm::mat4 OpenVrTrackingCompositor::GetViewProjectionMatrix(size_t view, float nearZ, float farZ) {
    // TODO: helper function that verifies "eye" is a sane value and returns vr::Eye_XXXX based on the value.
    vr::EVREye eyeType = (view == 0) ? vr::Eye_Left : vr::Eye_Right;

    vr::HmdMatrix44_t projMatrix = vrSystem->GetProjectionMatrix(eyeType, nearZ, farZ);
    return glm::make_mat4((float *)projMatrix.m);
}

std::vector<TrackedObjectHandle> OpenVrTrackingCompositor::GetTrackedObjectHandles() {
    // TODO: probably shouldn't run this logic on every frame
    std::vector<TrackedObjectHandle> connectedDevices = {
        {HMD, NONE, "xr-hmd", vrSystem->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd)},
    };

    return connectedDevices;
}

std::shared_ptr<XrModel> OpenVrTrackingCompositor::GetTrackedObjectModel(const TrackedObjectHandle &handle) {
    return OpenVrModel::LoadOpenVrModel(GetOpenVrIndexFromHandle(handle));
}

vr::TrackedDeviceIndex_t OpenVrTrackingCompositor::GetOpenVrIndexFromHandle(const TrackedObjectHandle &handle) {
    // Work out which model to load
    vr::TrackedDeviceIndex_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
    vr::ETrackedDeviceClass desiredClass = vr::TrackedDeviceClass_Invalid;
    vr::ETrackedControllerRole desiredRole = vr::TrackedControllerRole_Invalid;

    if (handle.type == CONTROLLER) {
        desiredClass = vr::TrackedDeviceClass_Controller;

        if (handle.hand == LEFT) {
            desiredRole = vr::TrackedControllerRole_LeftHand;
        } else if (handle.hand == RIGHT) {
            desiredRole = vr::TrackedControllerRole_RightHand;
        } else {
            Errorf("Loading models for ambidextrous controllers not supported");
            return vr::k_unTrackedDeviceIndexInvalid;
        }

        deviceIndex = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(desiredRole);
    } else if (handle.type == HMD) {
        deviceIndex = vr::k_unTrackedDeviceIndex_Hmd;
    } else {
        Errorf("Loading models for other types not yet supported");
        return vr::k_unTrackedDeviceIndexInvalid;
    }

    return deviceIndex;
}
