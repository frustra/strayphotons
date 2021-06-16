#include <core/CVar.hh>
#include <core/Logging.hh>
#include <graphics/opengl/GLTexture.hh>
#include <stdexcept>
#include <xr/openvr/OpenVrAction.hh>
#include <xr/openvr/OpenVrModel.hh>
#include <xr/openvr/OpenVrSystem.hh>

// GLM headers
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace sp;
using namespace xr;

static CVar<bool> CVarBindPose("r.BindPose", false, "Feed the SteamVR bind pose instead of real hand skeletons");

OpenVrActionSet::OpenVrActionSet(std::string setName, std::string description) : XrActionSet(setName, description) {
    vr::EVRInputError inputError = vr::VRInput()->GetActionSetHandle(setName.c_str(), &handle);

    if (inputError != vr::EVRInputError::VRInputError_None) {
        throw std::runtime_error("Failed to initialize OpenVr action set");
    }
}

std::shared_ptr<XrAction> OpenVrActionSet::CreateAction(std::string name, XrActionType type) {
    std::shared_ptr<XrAction> action = std::shared_ptr<OpenVrAction>(new OpenVrAction(name, type, shared_from_this()));
    XrActionSet::AddAction(action);
    return action;
}

void OpenVrActionSet::Sync() {
    vr::VRActiveActionSet_t activeActionSet;
    activeActionSet.ulActionSet = handle;
    activeActionSet.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;

    vr::EVRInputError inputError = vr::VRInput()->UpdateActionState(&activeActionSet,
                                                                    sizeof(vr::VRActiveActionSet_t),
                                                                    1);

    if (inputError != vr::EVRInputError::VRInputError_None) { Errorf("Failed to sync OpenVR actions"); }
}

OpenVrAction::OpenVrAction(std::string name, XrActionType type, std::shared_ptr<OpenVrActionSet> actionSet)
    : XrAction(name, type) {
    this->parentActionSet = actionSet;

    vr::EVRInputError inputError = vr::VRInput()->GetActionHandle(name.c_str(), &handle);

    if (inputError != vr::EVRInputError::VRInputError_None || handle == vr::k_ulInvalidActionHandle) {
        throw std::runtime_error("Failed to get OpenVR action handle");
    }

    // If we are a skeleton action, we _must_ be one of the two supported Skeleton actions
    if (type == xr::Skeleton) {
        if (name != xr::LeftHandSkeletonActionName && name != xr::RightHandSkeletonActionName) {
            throw std::runtime_error("Unknown skeleton action name");
        }
    }

    // If we are a skeleton or a pose, we can potentially pre-load the model to avoid stalling
    // the engine after loading the scene
    //
    // Note that the model might need to reload mid-game, which will cause stuttering since it's done on the main
    // thread. #41
    if (type == xr::Skeleton || type == xr::Pose) { GetInputSourceModel(); }
}

bool OpenVrAction::GetBooleanActionValue(std::string subpath, bool &value) {
    vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
    vr::EVRInputError inputError = vr::VRInput()->GetInputSourceHandle(subpath.c_str(), &inputHandle);

    if (inputError != vr::EVRInputError::VRInputError_None) {
        Errorf("Failed to get subpath for action");
        return false;
    }

    vr::InputDigitalActionData_t data;
    inputError = vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(vr::InputDigitalActionData_t), inputHandle);

    if (inputError != vr::EVRInputError::VRInputError_None) { return false; }

    if (!data.bActive) { return false; }

    value = data.bState;
    return true;
}

bool OpenVrAction::GetRisingEdgeActionValue(std::string subpath, bool &value) {
    vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
    vr::EVRInputError inputError = vr::VRInput()->GetInputSourceHandle(subpath.c_str(), &inputHandle);

    if (inputError != vr::EVRInputError::VRInputError_None) {
        Errorf("Failed to get subpath for action");
        return false;
    }

    vr::InputDigitalActionData_t data;
    inputError = vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(vr::InputDigitalActionData_t), inputHandle);

    if (inputError != vr::EVRInputError::VRInputError_None) { return false; }

    if (!data.bActive) { return false; }

    value = (data.bState && data.bChanged);
    return true;
}

bool OpenVrAction::GetFallingEdgeActionValue(std::string subpath, bool &value) {
    vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
    vr::EVRInputError inputError = vr::VRInput()->GetInputSourceHandle(subpath.c_str(), &inputHandle);

    if (inputError != vr::EVRInputError::VRInputError_None) {
        Errorf("Failed to get subpath for action");
        return false;
    }

    vr::InputDigitalActionData_t data;
    inputError = vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(vr::InputDigitalActionData_t), inputHandle);

    if (inputError != vr::EVRInputError::VRInputError_None) { return false; }

    if (!data.bActive) { return false; }

    value = ((!data.bState) && data.bChanged);
    return true;
}

bool OpenVrAction::GetPoseActionValueForNextFrame(std::string subpath, glm::mat4 &pose) {
    vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
    vr::EVRInputError inputError = vr::EVRInputError::VRInputError_None;

    if (!subpath.empty()) {
        inputError = vr::VRInput()->GetInputSourceHandle(subpath.c_str(), &inputHandle);

        if (inputError != vr::EVRInputError::VRInputError_None) {
            throw std::runtime_error("Failed to get subpath for action");
        }
    }

    vr::InputPoseActionData_t data;
    inputError = vr::VRInput()->GetPoseActionDataForNextFrame(handle,
                                                              vr::ETrackingUniverseOrigin::TrackingUniverseStanding,
                                                              &data,
                                                              sizeof(vr::InputPoseActionData_t),
                                                              inputHandle);

    if (inputError != vr::EVRInputError::VRInputError_None) {
        // TODO: consider not throwing here
        throw std::runtime_error("Failed to get pose data for device");
    }

    if (!data.bActive) { return false; }

    if (!data.pose.bPoseIsValid) { return false; }

    pose = glm::mat4(glm::make_mat3x4((float *)data.pose.mDeviceToAbsoluteTracking.m));
    return true;
}

bool OpenVrAction::GetSkeletonActionValue(std::vector<XrBoneData> &bones, bool withController) {
    vr::InputSkeletalActionData_t data;
    vr::EVRInputError inputError = vr::VRInput()->GetSkeletalActionData(handle,
                                                                        &data,
                                                                        sizeof(vr::InputSkeletalActionData_t));

    if (inputError != vr::EVRInputError::VRInputError_None) {
        throw std::runtime_error("Failed to get skeletal action data");
    }

    // No active skeleton available
    if (!data.bActive) { return false; }

    uint32_t boneCount = 0;
    inputError = vr::VRInput()->GetBoneCount(handle, &boneCount);

    if (inputError != vr::EVRInputError::VRInputError_None) {
        throw std::runtime_error("Failed to get skeletal action data");
    }

    std::vector<vr::VRBoneTransform_t> boneTransforms;
    boneTransforms.resize(boneCount);

    if (!CVarBindPose.Get()) {
        inputError = vr::VRInput()->GetSkeletalBoneData(
            handle,
            vr::EVRSkeletalTransformSpace::VRSkeletalTransformSpace_Model,
            withController ? vr::EVRSkeletalMotionRange::VRSkeletalMotionRange_WithController
                           : vr::EVRSkeletalMotionRange::VRSkeletalMotionRange_WithoutController,
            boneTransforms.data(),
            boneCount);
    } else {
        inputError = vr::VRInput()->GetSkeletalReferenceTransforms(
            handle,
            vr::EVRSkeletalTransformSpace::VRSkeletalTransformSpace_Model,
            vr::EVRSkeletalReferencePose::VRSkeletalReferencePose_BindPose,
            boneTransforms.data(),
            boneCount);
    }

    if (inputError != vr::EVRInputError::VRInputError_None) {
        throw std::runtime_error("Failed to get skeletal action data");
    }

    // Make the output vector big enough to hold all the bones
    // for the model that we've previously provided to the application.
    // If we have not previously provided the application a model, this will
    // do nothing.
    bones.resize(modelBoneData.size());

    for (int i = 0; i < modelBoneData.size(); i++) {
        bones[i].pos = glm::vec3(boneTransforms[modelBoneData[i].steamVrBoneIndex].position.v[0],
                                 boneTransforms[modelBoneData[i].steamVrBoneIndex].position.v[1],
                                 boneTransforms[modelBoneData[i].steamVrBoneIndex].position.v[2]);

        bones[i].rot = glm::quat(boneTransforms[modelBoneData[i].steamVrBoneIndex].orientation.w,
                                 boneTransforms[modelBoneData[i].steamVrBoneIndex].orientation.x,
                                 boneTransforms[modelBoneData[i].steamVrBoneIndex].orientation.y,
                                 boneTransforms[modelBoneData[i].steamVrBoneIndex].orientation.z);

        bones[i].inverseBindPose = modelBoneData[i].inverseBindPose;
    }

    return true;
}

std::shared_ptr<XrModel> OpenVrAction::GetInputSourceModel() {
    // Skeletons have their own model load code
    if (GetActionType() == xr::Skeleton) {
        std::string skeletonUniqueName = OpenVrSkeleton::ModelName(GetName());
        std::shared_ptr<XrModel> skeleton;

        if (cachedModels.count(skeletonUniqueName) != 0) {
            skeleton = cachedModels[skeletonUniqueName];
        } else {
            skeleton = OpenVrSkeleton::LoadOpenVrSkeleton(GetName());

            // Only cache the model if it loaded correctly
            // TODO: make the Asset loader handle model caching. #41
            if (skeleton) { cachedModels[skeletonUniqueName] = skeleton; }
        }

        if (skeleton) {
            // ComputeBoneLookupTable can fail if SteamVR Input isn't ready to give us a list of bone names yet
            // This happens separately from loading the skeleton model.
            // GameLogic will retry loading this input source model repeatedly, which will retry this section
            if (ComputeBoneLookupTable(skeleton)) { return skeleton; }
        }
    }
    // Poses (basically, input controllers) have their own model load code
    else if (GetActionType() == xr::Pose) {
        vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
        vr::EVRInputError inputError =
            vr::VRInput()->GetActionOrigins(parentActionSet->GetHandle(), handle, &inputHandle, 1);

        if (inputError != vr::EVRInputError::VRInputError_None) { return nullptr; }

        if (inputHandle == vr::k_ulInvalidInputValueHandle) {
            // No device connected or action is unbound.
            return nullptr;
        }

        vr::InputOriginInfo_t info;
        inputError = vr::VRInput()->GetOriginTrackedDeviceInfo(inputHandle, &info, sizeof(vr::InputOriginInfo_t));

        if (inputError != vr::EVRInputError::VRInputError_None) {
            Errorf("Failed to get device information for action %s", GetName());
            return nullptr;
        }

        std::string modelUniqueName = OpenVrModel::ModelName(info.trackedDeviceIndex);
        std::shared_ptr<XrModel> model;

        if (cachedModels.count(modelUniqueName) != 0) {
            model = cachedModels[modelUniqueName];
        } else {
            model = OpenVrModel::LoadOpenVrModel(info.trackedDeviceIndex);
        }

        return model;
    }

    // We only support models for Skeletons and Pose action types, since these
    // are the only ones that natively support getting position data
    return nullptr;
}

bool OpenVrAction::ComputeBoneLookupTable(std::shared_ptr<XrModel> xrModel) {
    std::shared_ptr<Model> model = std::dynamic_pointer_cast<Model>(xrModel);

    // We are performing the following translations:
    // 1. GLTF Joint Index -> GLTF Node Index (Stored in GLTF "joints" array)
    // 2. GLTF Node Index -> GLTF Node Name (Stored in each GLTF Node)
    // 3. Steam VR Bone Name -> Steam VR Bone Index (SteamVR provides this)
    //
    // We want to store the translation of (Joint Index) -> (SteamVR Bone ID)
    // so that our lookups can have high performance during each frame.
    // The SteamVR Hand Skeleton Models have the property that
    // GLTF Node Name == SteamVR Bone Name, so we can use this to translate
    // GLTF Joint Index -> Steam VR Bone Index

    // Get #1: GLTF Joint Index -> GLTF Node Index translations from the GLTF.
    // GLTF Joint Index is the position in the vector.
    // GLTF Node Index is the value at each position in the vector.
    std::vector<int> jointNodes = model->GetJointNodes();

    // Resize the translation table vector to match the number of joints the GLTF will reference
    modelBoneData.resize(jointNodes.size());

    // Get #3: Steam VR Bone Name -> Steam VR Bone Index.
    // Steam VR Bone Index is the position in the vector
    // Steam VR Bone Name is the value at that position.
    std::vector<std::string> steamVrBoneNames = GetSteamVRBoneNames();

    // For each joint in the model (pulled from the GLTF "joints" array...)
    for (int i = 0; i < jointNodes.size(); i++) {
        // Get the steamVrBone that corresponds to this joint
        auto steamVrBone = std::find(
            steamVrBoneNames.begin(),
            steamVrBoneNames.end(),
            model->GetNodeName(jointNodes[i]) // This is translation #2: GLTF Node Index -> GLTF Node Name
        );

        if (steamVrBone == steamVrBoneNames.end()) {
            // TODO: there might be a way to handle bones in the model that don't have a matching
            // bone in SteamVR. For example, it might be possible to use two-bone-IK to fill in
            // the missing data.
            Errorf("Cannot find matching SteamVR bone for model bone %s", model->GetNodeName(jointNodes[i]));
            return false;
        }

        // SteamVR Bone Index is the position in the steamVrBoneNames vector.
        modelBoneData[i].steamVrBoneIndex = std::distance(steamVrBoneNames.begin(), steamVrBone);

        // Store the inverse bind pose for this node, taken from the GLTF
        modelBoneData[i].inverseBindPose = model->GetInvBindPoseForNode(jointNodes[i]);
    }

    return true;
}

std::vector<std::string> OpenVrAction::GetSteamVRBoneNames() {
    uint32_t boneCount = 0;
    vr::EVRInputError inputError = vr::VRInput()->GetBoneCount(handle, &boneCount);

    if (inputError != vr::EVRInputError::VRInputError_None) {
        Errorf("Failed to get bone count for action skeleton");
        return std::vector<std::string>();
    }

    std::vector<std::string> boneNames;
    boneNames.resize(boneCount);

    for (int i = 0; i < boneCount; i++) {
        boneNames[i].resize(vr::k_unMaxBoneNameLength);
        vr::VRInput()->GetBoneName(handle, i, &boneNames[i].front(), vr::k_unMaxBoneNameLength);
        boneNames[i].erase(boneNames[i].find('\0'));
    }

    return boneNames;
}
