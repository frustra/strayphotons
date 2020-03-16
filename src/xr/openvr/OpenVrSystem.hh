#pragma once

#include "xr/XrSystem.hh"
#include "xr/openvr/OpenVrTrackingCompositor.hh"
#include "xr/openvr/OpenVrAction.hh"

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

			std::shared_ptr<XrActionSet> GetActionSet(std::string setName);

			std::vector<TrackedObjectHandle> GetTrackedObjectHandles();

			std::shared_ptr<XrModel> GetTrackedObjectModel(const TrackedObjectHandle &handle);

			static vr::TrackedDeviceIndex_t GetOpenVrIndexFromHandle(vr::IVRSystem *vrs, const TrackedObjectHandle &handle);

		private:
			vr::IVRSystem *vrSystem;
			std::shared_ptr<OpenVrTrackingCompositor> trackingCompositor;
			std::map<std::string, std::shared_ptr<OpenVrActionSet>> actionSets;
			char tempVrProperty[vr::k_unMaxPropertyStringSize];
		};

	} // namespace xr
} // namespace sp