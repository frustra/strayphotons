#pragma once

#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
	struct TriggerArea {
		glm::vec3 boundsMin, boundsMax;
		std::string command;
		bool triggered = false;
	};

	static Component<TriggerArea> ComponentTriggerArea("triggerarea"); // TODO: Rename this

	template<>
	bool Component<TriggerArea>::LoadEntity(Entity &dst, picojson::value &src);
} // namespace ecs
