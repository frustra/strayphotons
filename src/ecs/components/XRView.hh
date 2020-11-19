#pragma once

#include <ecs/Ecs.hh>

namespace ecs {
	struct XRView {
		int viewId;
	};

	static Component<XRView> ComponentXRView("xrview");
}; // namespace ecs
