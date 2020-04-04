#include <xr/openvr/OpenVrAction.hh>
#include <xr/openvr/OpenVrSystem.hh>
#include <xr/openvr/OpenVrModel.hh>

#include <stdexcept>

// GLM headers
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace sp;
using namespace xr;

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
    std::shared_ptr<XrAction> action = std::make_shared<OpenVrAction>(name, type);
    XrActionSet::AddAction(action);

    return action;
}

bool OpenVrActionSet::IsInputSourceConnected(std::string actionStr)
{
    std::shared_ptr<OpenVrAction> action = std::dynamic_pointer_cast<OpenVrAction>(XrActionSet::GetAction(actionStr));

	vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
	vr::EVRInputError inputError = vr::VRInput()->GetActionOrigins(handle, action->GetHandle(), &inputHandle, 1);

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

std::shared_ptr<XrModel> OpenVrActionSet::GetInputSourceModel(std::string actionStr)
{
    std::shared_ptr<OpenVrAction> action = std::dynamic_pointer_cast<OpenVrAction>(XrActionSet::GetAction(actionStr));

	vr::VRInputValueHandle_t inputHandle = vr::k_ulInvalidInputValueHandle;
	vr::EVRInputError inputError = vr::VRInput()->GetActionOrigins(handle, action->GetHandle(), &inputHandle, 1);

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

	return OpenVrModel::LoadOpenVRModel(info.trackedDeviceIndex);
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

OpenVrAction::OpenVrAction(std::string name, XrActionType type) : 
    XrAction(name, type)
{
    vr::EVRInputError inputError = vr::VRInput()->GetActionHandle(name.c_str(), &handle);

    if (inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to initialize OpenVr action set");
    }
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
    vr::EVRInputError inputError = vr::VRInput()->GetInputSourceHandle(subpath.c_str(), &inputHandle);

    if(inputError != vr::EVRInputError::VRInputError_None)
    {
        throw std::runtime_error("Failed to get subpath for action");
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

	pose = glm::make_mat3x4((float *)data.pose.mDeviceToAbsoluteTracking.m);

	return true;
}