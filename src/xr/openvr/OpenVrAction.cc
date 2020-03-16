#include <xr/openvr/OpenVrAction.hh>

#include <stdexcept>

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

