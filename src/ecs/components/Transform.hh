#pragma once

#include <glm/glm.hpp>

#include "ecs/Ecs.hh"

namespace ECS
{
	class Transform
	{
	public:
		Transform() {}
		void SetRelativeTo(sp::Entity);

		/**
		 * Return the matrix for specifying the placement of the entity in the world.
		 * This involves computing the incremental model transforms for any entities
		 * that this placement is relative to.
		 */
		glm::mat4 GetModelTransform(sp::EntityManager &manager);

		/**
		 * Change the local rotation by "radians" amount about the local "axis"
		 */
		void Rotate(float radians, glm::vec3 axis);

		/**
		 * Change the local position by an amount in the local x, y, z planes
		 */
		void Translate(glm::vec3 xyz);

		/**
		 * Change the local position to xyz
		 */
		void SetTransform(glm::vec3 xyz);

		/**
		 * Change the local scale by an amount in the local x, y, z planes.
		 * This will propagate to Transforms that are relative to this one.
		 */
		void Scale(glm::vec3 xyz);

	private:
		glm::mat4 position;
		glm::mat4 scale;
		glm::mat4 rotate;
		sp::Entity::Id relativeTo;
	};
}