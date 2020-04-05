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
