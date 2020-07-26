#pragma once

#include "ecs/EntityManager.hh"
#include "xr/XrCompositor.hh"
#include "xr/XrTracking.hh"

#include <openvr.h>

namespace sp {

	namespace xr {
		// In OpenVR, the tracking and compositor are too interlinked to separate cleanly.
		// This class implements both the XrTracking and XrCompositor functionality so that
		// state can be shared between tracking and compositor code.
		class OpenVrTrackingCompositor : public XrTracking, public XrCompositor {
		public:
			OpenVrTrackingCompositor(vr::IVRSystem *vrs);

			// XrTracking functions
			bool GetPredictedViewPose(size_t view, glm::mat4 &viewPose);
			bool GetPredictedObjectPose(const TrackedObjectHandle &handle, glm::mat4 &objectPose);
			std::vector<TrackedObjectHandle> GetTrackedObjectHandles();
			std::shared_ptr<XrModel> GetTrackedObjectModel(const TrackedObjectHandle &handle);

			// XrCompositor functions
			size_t GetNumViews(bool minimum = true);
			void GetRecommendedRenderTargetSize(uint32_t &width, uint32_t &height);
			glm::mat4 GetViewProjectionMatrix(size_t view, float nearZ, float farZ);
			RenderTarget::Ref GetRenderTarget(size_t view);
			void PopulateView(size_t view, ecs::Handle<ecs::View> ecsView);
			void SubmitView(size_t view, RenderTarget::Ref rt);
			void WaitFrame();
			void BeginFrame();
			void EndFrame();

		private:
			vr::TrackedDeviceIndex_t GetOpenVrIndexFromHandle(const TrackedObjectHandle &handle);

			vr::IVRSystem *vrSystem;
			std::vector<std::shared_ptr<RenderTarget>> viewRenderTargets;
		};

	} // namespace xr
} // namespace sp