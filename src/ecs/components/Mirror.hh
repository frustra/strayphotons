#pragma once

#include <glm/glm.hpp>

#include <ecs/Components.hh>

namespace ecs
{
	struct Mirror
	{
		glm::vec2 size;
		int mirrorId;
	};

	static Component<Mirror> ComponentMirror("mirror");

	template<>
	bool Component<Mirror>::LoadEntity(Entity &dst, picojson::value &src);
}
