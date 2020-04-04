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
#include <filesystem>
#include <memory>

using namespace sp;
using namespace xr;
namespace fs = std::filesystem;

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

	// Initialize SteamVR Input subsystem
	fs::path cwd = fs::current_path();
	cwd /= "actions.json";
	cwd = fs::absolute(cwd);

	std::string action_path = cwd.string();

	vr::EVRInputError inputError = vr::VRInput()->SetActionManifestPath(action_path.c_str());

	if (inputError != vr::EVRInputError::VRInputError_None)
	{
		throw std::runtime_error("Failed to init SteamVR input");
	}
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

std::shared_ptr<XrActionSet> OpenVrSystem::GetActionSet(std::string setName)
{
	if (actionSets.count(setName) == 0)
	{
		actionSets[setName] = make_shared<OpenVrActionSet>(setName, "A SteamVr Action Set");
	}

	return actionSets[setName];
}

std::vector<TrackedObjectHandle> OpenVrSystem::GetTrackedObjectHandles()
{
	// TODO: probably shouldn't run this logic on every frame
	std::vector<TrackedObjectHandle> connectedDevices =
	{
		{HMD, NONE, "xr-hmd", vrSystem->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd)},
	};

	return connectedDevices;
}

std::shared_ptr<XrModel> OpenVrSystem::GetTrackedObjectModel(const TrackedObjectHandle &handle)
{
	return OpenVrModel::LoadOpenVRModel(GetOpenVrIndexFromHandle(handle));
}

vr::TrackedDeviceIndex_t OpenVrSystem::GetOpenVrIndexFromHandle(const TrackedObjectHandle &handle)
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

		deviceIndex = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(desiredRole);
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
