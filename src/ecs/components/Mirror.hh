#pragma once

#include <ecs/Ecs.hh>
#include <glm/glm.hpp>

namespace ecs {
	struct Mirror {
		glm::vec2 size;
		int mirrorId;
	};

	static Component<Mirror> ComponentMirror("mirror");

	template<>
	bool Component<Mirror>::LoadEntity(Entity &dst, picojson::value &src);
} // namespace ecs
