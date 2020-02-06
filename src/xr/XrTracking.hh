#pragma once

#include "xr/XrModel.hh"
#include <glm/glm.hpp> // For glm::mat4

namespace sp
{

	namespace xr
	{

		// This class encapsulates tracking functionality for the HMD and any associated Motion Controllers.
		// Conceptually, this means it must:
		//  - Provide methods to get the HMD pose at a particular time
		//  - Provide methods to initialize Motion Controller objects
		//  - Provide methods to get predicted Motion Controller poses

		class XrTracking
		{
		public:
			// Provides the predicted pose of a View during the current frame.
			// Must be called after XrCompositor::WaitFrame() or you will end up
			// with predicted HMD poses from the previous frame.
			virtual bool GetPredictedViewPose(size_t view, glm::mat4 &viewPose) = 0;

			// Provides the predicted pose of a tracked object during the _NEXT_ frame.
			// This is used for gameplay. The sequence is:
			// 1. XrCompositor::WaitFrame() (computes tracking data for views at Frame N, controllers at frame N+1)
			// 2. Render Scene
			// 3. XrCompositor::EndFrame()
			// 4. Move controller model to position from 1.
			// 5. GOTO 1
			virtual bool GetPredictedObjectPose(const TrackedObjectHandle &handle, glm::mat4 &objectPose) = 0;
		};
	}
}
