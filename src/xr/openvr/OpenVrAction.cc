#include <xr/openvr/OpenVrAction.hh>
#include <xr/openvr/OpenVrSystem.hh>
#include <xr/openvr/OpenVrModel.hh>
#include <core/CVar.hh>

#include <stdexcept>

// GLM headers
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace sp;
using namespace xr;

static CVar<bool> CVarBindPose("r.BindPose", false, "Feed the SteamVR bind pose instead of real hand skeletons");

OpenVrActionSet::OpenVrActionSet(std::string setName, std::string description) : 
    XrActionSet(setName, description)
{
    vr::EVRInputError inputError = vr::VRInput()->GetActionSetHandle(setName.c_str(), &handle);

    if (inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to initialize OpenVr action set");
    }
}

std::shared_ptr<XrAction> OpenVrActionSet::CreateAction(std::string name, XrActionType type)
{
    std::shared_ptr<XrAction> action = std::shared_ptr<OpenVrAction>(new OpenVrAction(name, type, shared_from_this()));
    XrActionSet::AddAction(action);
    return action;
}

void OpenVrActionSet::Sync()
{
	vr::VRActiveActionSet_t activeActionSet;
	activeActionSet.ulActionSet = handle;
	activeActionSet.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle; // No restriction on hand syncing

	vr::EVRInputError inputError = vr::VRInput()->UpdateActionState(&activeActionSet, sizeof(vr::VRActiveActionSet_t), 1);

	if (inputError != vr::EVRInputError::VRInputError_None)
	{
		throw std::runtime_error("Failed to sync actions");
	}
}

OpenVrAction::OpenVrAction(std::string name, XrActionType type, std::shared_ptr<OpenVrActionSet> actionSet) : 
    XrAction(name, type)
{
    this->parentActionSet = actionSet;

    vr::EVRInputError inputError = vr::VRInput()->GetActionHandle(name.c_str(), &handle);

    if (inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to initialize OpenVr action set");
    }

    // TODO:
    // If we are a skeleton action, we _must_ be one of the two supported Skeleton actions

    // TODO:
    // If we are a skeleton, we can preload the "joint tranlation table" that we will need to use
    // to convert "SteamVR Input" bones into "model bones" that are referenced in the GLTF.
}

bool OpenVrAction::GetBooleanActionValue(std::string subpath)
{
    vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
    vr::EVRInputError inputError = vr::VRInput()->GetInputSourceHandle(subpath.c_str(), &inputHandle);

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get subpath for action");
    }

    vr::InputDigitalActionData_t data;
    inputError = vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(vr::InputDigitalActionData_t), inputHandle);

    if(inputError == vr::EVRInputError::VRInputError_None)
    {
        return data.bState;
    }

    return false;
}

bool OpenVrAction::GetRisingEdgeActionValue(std::string subpath)
{
    vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
    vr::EVRInputError inputError = vr::VRInput()->GetInputSourceHandle(subpath.c_str(), &inputHandle);

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get subpath for action");
    }

	vr::InputDigitalActionData_t data;
    inputError = vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(vr::InputDigitalActionData_t), inputHandle);

    if(inputError == vr::EVRInputError::VRInputError_None)
    {
        return data.bState && data.bChanged;
    }

    return false;
}

bool OpenVrAction::GetFallingEdgeActionValue(std::string subpath)
{
    vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
    vr::EVRInputError inputError = vr::VRInput()->GetInputSourceHandle(subpath.c_str(), &inputHandle);

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get subpath for action");
    }

	vr::InputDigitalActionData_t data;
    inputError = vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(vr::InputDigitalActionData_t), inputHandle);

    if(inputError == vr::EVRInputError::VRInputError_None)
    {
        return (!data.bState) && data.bChanged;
    }

    return false;
}

bool OpenVrAction::GetPoseActionValueForNextFrame(std::string subpath, glm::mat4 &pose)
{
    vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
    vr::EVRInputError inputError = vr::EVRInputError::VRInputError_None;
    
    if (!subpath.empty())
    {
        inputError = vr::VRInput()->GetInputSourceHandle(subpath.c_str(), &inputHandle);

        if(inputError != vr::EVRInputError::VRInputError_None)
        {
            throw std::runtime_error("Failed to get subpath for action");
        }
    }
    
	vr::InputPoseActionData_t data;
    inputError = vr::VRInput()->GetPoseActionDataForNextFrame(handle, vr::ETrackingUniverseOrigin::TrackingUniverseStanding, &data, sizeof(vr::InputPoseActionData_t), inputHandle);

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        // TODO: consider not throwing here
        throw std::runtime_error("Failed to get pose data for device");
    }

	if (!data.pose.bPoseIsValid)
	{
		return false;
	}

	pose = glm::mat4(glm::make_mat3x4((float *)data.pose.mDeviceToAbsoluteTracking.m));

	return true;
}

bool OpenVrAction::GetSkeletonActionValue(std::vector<XrBoneData> &bones, bool withController)
{    
	vr::InputSkeletalActionData_t data;
    vr::EVRInputError inputError = vr::VRInput()->GetSkeletalActionData(handle, &data, sizeof(vr::InputSkeletalActionData_t));

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get skeletal action data");
    }

    // No active skeleton available
	if (!data.bActive)
	{
		return false;
	}

    uint32_t boneCount = 0;
    inputError = vr::VRInput()->GetBoneCount(handle, &boneCount);

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get skeletal action data");
    }

    std::vector<vr::VRBoneTransform_t> boneTransforms;
    boneTransforms.resize(boneCount);

    if (!CVarBindPose.Get())
    {
        inputError = vr::VRInput()->GetSkeletalBoneData(
            handle, 
            vr::EVRSkeletalTransformSpace::VRSkeletalTransformSpace_Model, 
            withController ? vr::EVRSkeletalMotionRange::VRSkeletalMotionRange_WithController : vr::EVRSkeletalMotionRange::VRSkeletalMotionRange_WithoutController,
            boneTransforms.data(),
            boneCount
        );
    }
    else
    {
        inputError = vr::VRInput()->GetSkeletalReferenceTransforms(
            handle, 
            vr::EVRSkeletalTransformSpace::VRSkeletalTransformSpace_Model, 
            vr::EVRSkeletalReferencePose::VRSkeletalReferencePose_BindPose, 
            boneTransforms.data(), 
            boneCount
        );
    }

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get skeletal action data");
    }
    
    // Make the output vector big enough to hold all the bones
    // for the model that we've previously provided to the application.
    // If we have not previously provided the application a model, this will
    // do nothing.
    bones.resize(modelBoneData.size());

    for(int i = 0; i < modelBoneData.size(); i++)
    {
        bones[i].pos  = glm::vec3(
            boneTransforms[modelBoneData[i].steamVrBoneIndex].position.v[0],
            boneTransforms[modelBoneData[i].steamVrBoneIndex].position.v[1],
            boneTransforms[modelBoneData[i].steamVrBoneIndex].position.v[2]);

        bones[i].rot  = glm::quat(
            boneTransforms[modelBoneData[i].steamVrBoneIndex].orientation.w,
            boneTransforms[modelBoneData[i].steamVrBoneIndex].orientation.x,
            boneTransforms[modelBoneData[i].steamVrBoneIndex].orientation.y,
            boneTransforms[modelBoneData[i].steamVrBoneIndex].orientation.z);

        bones[i].inverseBindPose = modelBoneData[i].inverseBindPose;
    }

	return true;
}

bool OpenVrAction::IsInputSourceConnected()
{
	vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
	vr::EVRInputError inputError = vr::VRInput()->GetActionOrigins(parentActionSet->GetHandle(), handle, &inputHandle, 1);

    // We only support one binding source, this is bound to multiple sources
    if(inputError == vr::EVRInputError::VRInputError_BufferTooSmall)
    {
        return false;
    }
	else if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get subpath for action");
    }

    if (inputHandle == vr::k_ulInvalidInputValueHandle)
    {
        // No device connected or action is unbound.
        return false;
    }

    vr::InputOriginInfo_t info;
	inputError = vr::VRInput()->GetOriginTrackedDeviceInfo(inputHandle, &info, sizeof(vr::InputOriginInfo_t));

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get device info");
    }

	return info.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid;
}

std::shared_ptr<XrModel> OpenVrAction::GetInputSourceModel()
{
    // Skeletons require special model loading
    if (this->GetActionType() == xr::Skeleton)
    {
        std::shared_ptr<XrModel> skeleton = OpenVrSkeleton::LoadOpenVrSkeleton(this->GetName());

        if (skeleton)
        {
            ComputeBoneLookupTable(skeleton);
            return skeleton;
        }

        return false;        
    }
    else
    {
        vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
        vr::EVRInputError inputError = vr::VRInput()->GetActionOrigins(parentActionSet->GetHandle(), handle, &inputHandle, 1);

        if(inputError != vr::EVRInputError::VRInputError_None)
        {
            throw std::runtime_error("Failed to get subpath for action");
        }

        if (inputHandle == vr::k_ulInvalidInputValueHandle)
        {
            // No device connected or action is unbound.
            return false;
        }

        vr::InputOriginInfo_t info;
        inputError = vr::VRInput()->GetOriginTrackedDeviceInfo(inputHandle, &info, sizeof(vr::InputOriginInfo_t));

        if(inputError != vr::EVRInputError::VRInputError_None)
        {
            throw std::runtime_error("Failed to get device info");
        }

        return OpenVrModel::LoadOpenVrModel(info.trackedDeviceIndex);
    }	
}

void OpenVrAction::ComputeBoneLookupTable(std::shared_ptr<XrModel> xrModel)
{
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
    for(int i = 0; i < jointNodes.size(); i++)
    {			
        // Get the steamVrBone that corresponds to this joint
        auto steamVrBone = std::find(
            steamVrBoneNames.begin(), 
            steamVrBoneNames.end(), 
            model->GetNodeName(jointNodes[i]) // This is translation #2: GLTF Node Index -> GLTF Node Name
        );

        if (steamVrBone == steamVrBoneNames.end())
        {
            // TODO: there might be a way to handle bones in the model that don't have a matching
            // bone in SteamVR. For example, it might be possible to use two-bone-IK to fill in
            // the missing data.
            throw std::runtime_error("Cannot find matching SteamVR bone for model bone");
        }

        // SteamVR Bone Index is the position in the steamVrBoneNames vector.
        modelBoneData[i].steamVrBoneIndex = std::distance(steamVrBoneNames.begin(), steamVrBone);

        // Store the inverse bind pose for this node, taken from the GLTF
        modelBoneData[i].inverseBindPose = model->GetInvBindPoseForNode(jointNodes[i]);
    }
}

std::vector<std::string> OpenVrAction::GetSteamVRBoneNames()
{
    uint32_t boneCount = 0;
    vr::EVRInputError inputError = vr::VRInput()->GetBoneCount(handle, &boneCount);

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get bone count for action skeleton");
    }

    std::vector<std::string> boneNames;
    boneNames.resize(boneCount);

    for (int i = 0; i < boneCount; i++)
    {
        // TODO: bone name max length is not specified by SteamVR spec.
        boneNames[i].resize(255);
        vr::VRInput()->GetBoneName(handle, i, &boneNames[i].front(), 255);
        boneNames[i].erase(boneNames[i].find('\0'));
    }

    return boneNames;
}