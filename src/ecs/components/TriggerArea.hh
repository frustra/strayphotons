#pragma once

#include <glm/glm.hpp>

#include <ecs/Components.hh>

namespace ecs
{
	struct TriggerArea
	{
		glm::vec3 boundsMin, boundsMax;
		std::string command;
		bool triggered = false;
	};

	static Component<TriggerArea> ComponentTriggerArea("triggerarea"); // TODO: Rename this

	template<>
	bool Component<TriggerArea>::LoadEntity(Entity &dst, picojson::value &src);
}
