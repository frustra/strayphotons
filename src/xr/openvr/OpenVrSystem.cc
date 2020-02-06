// Project Headers
#include "xr/openvr/OpenVrSystem.hh"
#include "xr/openvr/OpenVrModel.hh"
#include "Common.hh"

// OpenVR headers
#include <openvr.h>

// GLM headers
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// System Headers
#include <stdexcept>
#include <thread>

using namespace sp;
using namespace xr;

OpenVrSystem::OpenVrSystem() :
	vrSystem(nullptr)
{
	// No init at this time
}

OpenVrSystem::~OpenVrSystem()
{
	// Tracking / compositor uses the vrSystem pointer. Must destroy it first
	trackingCompositor.reset();

	if (IsInitialized())
	{
		Deinit();
	}
}

void OpenVrSystem::Init()
{
	// Already initialized
	if (vrSystem)
	{
		return;
	}

	vr::EVRInitError err = vr::VRInitError_None;
	vrSystem = vr::VR_Init(&err, vr::VRApplication_Scene);

	if (err != vr::VRInitError_None)
	{
		vrSystem = nullptr;
		throw std::runtime_error(VR_GetVRInitErrorAsSymbol(err));
	}

	// Initialize the tracking / compositor subsystem
	trackingCompositor = make_shared<OpenVrTrackingCompositor>(vrSystem);
}

bool OpenVrSystem::IsInitialized()
{
	return (vrSystem != nullptr);
}

void OpenVrSystem::Deinit()
{
	// Not initialized yet
	if (!vrSystem)
	{
		throw std::runtime_error("OpenVR not yet initialized");
	}

	vr::VR_Shutdown();
	vrSystem = nullptr;
}

bool OpenVrSystem::IsHmdPresent()
{
	return vr::VR_IsRuntimeInstalled() && vr::VR_IsHmdPresent();
}

std::shared_ptr<XrTracking> OpenVrSystem::GetTracking()
{
	return trackingCompositor;
}

std::shared_ptr<XrCompositor> OpenVrSystem::GetCompositor()
{
	return trackingCompositor;
}

std::vector<TrackedObjectHandle> OpenVrSystem::GetTrackedObjectHandles()
{
	// TODO: probably shouldn't run this logic on every frame
	std::vector<TrackedObjectHandle> connectedDevices =
	{
		{HMD, NONE, "xr-hmd", false},
		{CONTROLLER, LEFT, "xr-controller-left", false},
		{CONTROLLER, RIGHT, "xr-controller-right", false},
	};

	if (vrSystem->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd))
	{
		connectedDevices[0].connected = true;
	}

	if (vrSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand) != vr::k_unTrackedDeviceIndexInvalid)
	{
		connectedDevices[1].connected = true;
	}

	if (vrSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand) != vr::k_unTrackedDeviceIndexInvalid)
	{
		connectedDevices[2].connected = true;
	}

	return connectedDevices;
}

std::shared_ptr<XrModel> OpenVrSystem::GetTrackedObjectModel(const TrackedObjectHandle &handle)
{
	// Work out which model to load
	vr::TrackedDeviceIndex_t deviceIndex = GetOpenVrIndexFromHandle(vrSystem, handle);

	if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid)
	{
		throw std::runtime_error("Failed to get tracked device index for TrackedObjectHandle");
	}

	vrSystem->GetStringTrackedDeviceProperty(deviceIndex, vr::Prop_RenderModelName_String, tempVrProperty, vr::k_unMaxPropertyStringSize);

	Logf("Loading VR render model %s", tempVrProperty);
	vr::RenderModel_t *vrModel;
	vr::RenderModel_TextureMap_t *vrTex;
	vr::EVRRenderModelError merr;

	while (true)
	{
		merr = vr::VRRenderModels()->LoadRenderModel_Async(tempVrProperty, &vrModel);
		if (merr != vr::VRRenderModelError_Loading) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (merr != vr::VRRenderModelError_None)
	{
		Errorf("VR render model error: %s", vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(merr));
		throw std::runtime_error("Failed to load VR render model");
	}

	while (true)
	{
		merr = vr::VRRenderModels()->LoadTexture_Async(vrModel->diffuseTextureId, &vrTex);
		if (merr != vr::VRRenderModelError_Loading) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (merr != vr::VRRenderModelError_None)
	{
		Errorf("VR render texture error: %s", vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(merr));
		throw std::runtime_error("Failed to load VR render texture");
	}

	std::shared_ptr<XrModel> xrModel = make_shared<OpenVrModel>(vrModel, vrTex);

	vr::VRRenderModels()->FreeTexture(vrTex);
	vr::VRRenderModels()->FreeRenderModel(vrModel);

	return xrModel;
}

vr::TrackedDeviceIndex_t OpenVrSystem::GetOpenVrIndexFromHandle(vr::IVRSystem *vrs, const TrackedObjectHandle &handle)
{
	// Work out which model to load
	vr::TrackedDeviceIndex_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
	vr::ETrackedDeviceClass desiredClass = vr::TrackedDeviceClass_Invalid;
	vr::ETrackedControllerRole desiredRole = vr::TrackedControllerRole_Invalid;

	if (handle.type == CONTROLLER)
	{
		desiredClass = vr::TrackedDeviceClass_Controller;

		if (handle.hand == LEFT)
		{
			desiredRole = vr::TrackedControllerRole_LeftHand;
		}
		else if (handle.hand == RIGHT)
		{
			desiredRole = vr::TrackedControllerRole_RightHand;
		}
		else
		{
			throw std::runtime_error("Loading models for ambidextrous controllers not supported");
		}

		deviceIndex = vrs->GetTrackedDeviceIndexForControllerRole(desiredRole);
	}
	else if (handle.type == HMD)
	{
		deviceIndex = vr::k_unTrackedDeviceIndex_Hmd;
	}
	else
	{
		throw std::runtime_error("Loading models for other types not yet supported");
	}

	return deviceIndex;
}

std::shared_ptr<OpenVrInputSource> OpenVrSystem::GetInteractionSourceFromManufacturer(vr::TrackedDeviceIndex_t deviceIndex)
{
	// Determine the manufacturer of this device.
	// Potentially supported options: HTC, Oculus, Valve
	vr::ETrackedPropertyError error;
	vrSystem->GetStringTrackedDeviceProperty(deviceIndex, vr::Prop_ManufacturerName_String, tempVrProperty, vr::k_unMaxPropertyStringSize, &error);

	if (strcmp(tempVrProperty, "Oculus") == 0)
	{
		Logf("Creating OculusInputSource");
		return make_shared<OculusInputSource>();
	}
	else if (strcmp(tempVrProperty, "HTC") == 0)
	{
		Logf("Creating ViveInputSource");
		return make_shared<ViveInputSource>();
	}
	else if (strcmp(tempVrProperty, "Valve") == 0)
	{
		Logf("Creating IndexInputSource");
		return make_shared<IndexInputSource>();
	}

	// No input source...
	Errorf("Failed to determine OpenVrInputSource for name: %s", tempVrProperty);
	return std::shared_ptr<NullInputSource>();
}

void OpenVrSystem::SyncActions(XrActionSet &actionSet)
{
	vr::TrackedDeviceIndex_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
	deviceIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
	if (deviceIndex != vr::k_unTrackedDeviceIndexInvalid)
	{
		if (!inputSources[0])
		{
			inputSources[0] = GetInteractionSourceFromManufacturer(deviceIndex);
		}

		vrSystem->GetControllerState(deviceIndex, &controllerState[0], sizeof(vr::VRControllerState_t));
	}
	else
	{
		inputSources[0].reset();
	}

	deviceIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
	if (deviceIndex != vr::k_unTrackedDeviceIndexInvalid)
	{
		if (!inputSources[1])
		{
			inputSources[1] = GetInteractionSourceFromManufacturer(deviceIndex);
		}

		vrSystem->GetControllerState(deviceIndex, &controllerState[1], sizeof(vr::VRControllerState_t));
	}
	else
	{
		inputSources[1].reset();
	}
}

void OpenVrSystem::GetActionState(std::string actionName, xr::XrActionSet &actionSet, std::string subpath)
{
	std::shared_ptr<XrActionBase> action = (actionSet.GetActionMap())[actionName];

	// No subpath on this action. Nothing to sync
	if (!action->HasSubpath(subpath))
	{
		return;
	}

	XrAction<XrActionType::Bool> *boolAction = dynamic_cast<XrAction<XrActionType::Bool>*>(action.get());

	if (boolAction)
	{
		// Set edge_val to the "old" value
		// This allows the edge detector to work out when we transition between states
		boolAction->actionData[subpath].edge_val = boolAction->actionData[subpath].value;
	}

	action->Reset(subpath);

	// TODO: requires rearchitechting to fix this.
	// Since this is mostly throw away, just use controller 0's profile for now.
	std::string interactionProfile = inputSources[0]->GetInteractionProfile();

	for (std::string binding : action->GetSuggestedBindings()[interactionProfile])
	{
		if (starts_with(binding, subpath))
		{
			if (boolAction)
			{
				boolAction->actionData[subpath].value |= GetInputSourceState(binding);
			}
		}
	}
}

std::string OpenVrSystem::GetInteractionProfile()
{
	return "/interaction_profiles/oculus/touch_controller";
}

bool OpenVrSystem::GetInputSourceState(std::string inputSourcePath)
{
	vr::VRControllerState_t *state;
	std::shared_ptr<OpenVrInputSource> inputSource;
	string prefix;

	// IF OCULUS TOUCH CONTROLLER
	if (starts_with(inputSourcePath, "/user/hand/left"))
	{
		prefix = "/user/hand/left/";
		state = &controllerState[0];
		inputSource = inputSources[0];
	}
	else if (starts_with(inputSourcePath, "/user/hand/right"))
	{
		prefix = "/user/hand/right/";
		state = &controllerState[1];
		inputSource = inputSources[1];

	}
	else
	{
		throw std::runtime_error("Unknown binding prefix");
	}

	if (inputSource)
	{
		return inputSource->GetInputSourceState(inputSourcePath, prefix, state);
	}

	return false;
}