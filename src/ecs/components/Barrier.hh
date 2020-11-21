#pragma once

#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace sp {
	class PhysxManager;
}

namespace ecs {
	class EntityManager;

	struct Barrier {
		bool isOpen = false;

		/**
		 * Creates a barrier that starts closed.
		 */
		static Entity Create(
			const glm::vec3 &pos, const glm::vec3 &dimensions, sp::PhysxManager &px, EntityManager &em);

		static void Close(Entity e, sp::PhysxManager &px);
		static void Open(Entity e, sp::PhysxManager &px);
	};

	static Component<Barrier> ComponentBarrier("barrier");

	template<>
	bool Component<Barrier>::LoadEntity(Entity &dst, picojson::value &src);
} // namespace ecs
