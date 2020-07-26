#pragma once

#include <ecs/Components.hh>

namespace ecs {
	struct XRView {
		int viewId;
	};

	static Component<XRView> ComponentXRView("xrview");
}; // namespace ecs