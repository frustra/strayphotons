#pragma once

#include "xr/XrSystem.hh"
#include "xr/openvr/OpenVrAction.hh"
#include "xr/openvr/OpenVrTrackingCompositor.hh"

#include <openvr.h>

namespace sp {

	namespace xr {
		class OpenVrSystem : public XrSystem {
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

		private:
			vr::IVRSystem *vrSystem;
			std::shared_ptr<OpenVrTrackingCompositor> trackingCompositor;
			std::map<std::string, std::shared_ptr<OpenVrActionSet>> actionSets;
		};

	} // namespace xr
} // namespace sp