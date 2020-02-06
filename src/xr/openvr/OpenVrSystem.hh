#pragma once

#include "xr/XrSystem.hh"
#include "xr/openvr/OpenVrTrackingCompositor.hh"
#include "xr/openvr/OpenVrInputSources.hh"

#include <openvr.h>

namespace sp
{

	namespace xr
	{
		class OpenVrSystem : public XrSystem
		{
		public:
			OpenVrSystem();
			~OpenVrSystem();

			void Init();

			bool IsInitialized();

			void Deinit();

			bool IsHmdPresent();

			std::shared_ptr<XrTracking> GetTracking();

			std::shared_ptr<XrCompositor> GetCompositor();

			std::vector<TrackedObjectHandle> GetTrackedObjectHandles();

			std::shared_ptr<XrModel> GetTrackedObjectModel(const TrackedObjectHandle &handle);

			static vr::TrackedDeviceIndex_t GetOpenVrIndexFromHandle(vr::IVRSystem *vrs, const TrackedObjectHandle &handle);

			void SyncActions(XrActionSet &actionSet);

			void GetActionState(std::string actionName, XrActionSet &actionSet, std::string subpath = "");

			std::string GetInteractionProfile();

			bool GetInputSourceState(std::string inputSource);

			std::shared_ptr<OpenVrInputSource> GetInteractionSourceFromManufacturer(vr::TrackedDeviceIndex_t deviceIndex);

		private:
			vr::IVRSystem *vrSystem;
			std::shared_ptr<OpenVrTrackingCompositor> trackingCompositor;
			vr::VRControllerState_t controllerState[2];
			std::shared_ptr<OpenVrInputSource> inputSources[2];
			char tempVrProperty[vr::k_unMaxPropertyStringSize];
		};

	} // namespace xr
} // namespace sp