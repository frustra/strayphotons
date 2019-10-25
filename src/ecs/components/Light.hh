#pragma once

#include <glm/glm.hpp>

#include <Ecs.hh>
#include <ecs/Components.hh>
#include <ecs/NamedEntity.hh>

namespace ecs
{
	struct Light
	{
		float spotAngle, intensity, illuminance;
		glm::vec3 tint;
		glm::vec4 mapOffset;
		int gelId;
		int lightId;
		bool on = true;
		NamedEntity bulb;
	};

	static Component<Light> ComponentLight("light");

	template<>
	bool Component<Light>::LoadEntity(Entity &dst, picojson::value &src);
}
