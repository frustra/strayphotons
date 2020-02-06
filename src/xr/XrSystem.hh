#pragma once

#include <memory> // for shared_ptr
#include <stdint.h> // For uint32_t
#include <glm/glm.hpp> // For glm::mat4

#include "xr/XrTracking.hh"
#include "xr/XrCompositor.hh"
#include "xr/XrModel.hh"
#include "xr/XrAction.hh"

namespace sp
{

	namespace xr
	{
		// This class is used to interact with the current XR backend's Runtime implementation.
		// All XR System implementations must support this interface.

		class XrSystem
		{
		public:
			// Initializes the XrSystem. Throws an exception on failure.
			virtual void Init() = 0;

			// Returns true if the VR system has been initialized.
			virtual bool IsInitialized() = 0;

			// Deinitializes the XrSystem. Throws an exception on failure.
			virtual void Deinit() = 0;

			// Quickly determine if a VR HMD is present. For some XR runtimes, this will always return true.
			// For example, if the runtime is specific to a self-contained XR HMD, it can be hard coded to return true.
			// On OpenVR, this could call vr::VR_IsHmdPresent() and/or vr::VR_IsRuntimeInstalled().
			// This function MUST NOT require XrSystem::Init() to have been called.
			virtual bool IsHmdPresent() = 0;

			virtual std::shared_ptr<XrTracking> GetTracking() = 0;

			virtual std::shared_ptr<XrCompositor> GetCompositor() = 0;

			virtual std::vector<TrackedObjectHandle> GetTrackedObjectHandles() = 0;

			virtual std::shared_ptr<XrModel> GetTrackedObjectModel(const TrackedObjectHandle &handle) = 0;

			// XrAction manipulation
			// TODO: consider moving into a separate XrInteraction class

			// This function does several things:
			// - Reads and caches the state of the underlying XR runtime's input system
			// - Populates all XrActions inside the action set with values
			// If any XrActions within the action set cannot be populated, throws a std::runtime_error
			virtual void SyncActions(XrActionSet &actionSet) = 0;

			virtual void GetActionState(std::string actionName, XrActionSet &actionSet, std::string subpath = "") = 0;
		};

	} // namespace xr
} // namespace sp