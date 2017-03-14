#pragma once

#include "Common.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Ecs.hh>

namespace ecs
{
	class Transform
	{
	public:
		Transform(ecs::EntityManager *manager) : manager(manager), dirty(false) {}
		void SetParent(ecs::Entity);
		bool HasParent();

		/**
		 * Return the transformation matrix including all parent transforms.
		 */
		glm::mat4 GetGlobalTransform() const;

		/**
		 * Returns the same as GetGlobalTransform() but only includes rotation components.
		 */
		glm::quat GetGlobalRotation() const;

		/**
		 * Get position including any transforms this is relative to
		 */
		glm::vec3 GetGlobalPosition() const;

		/**
		 * Get forward vector including any transforms this is relative to
		 */
		glm::vec3 GetGlobalForward() const;

		/**
		 * Change the local position by an amount in the local x, y, z planes
		 */
		void Translate(glm::vec3 xyz);

		/**
		 * Change the local rotation by "radians" amount about the local "axis"
		 */
		void Rotate(float radians, glm::vec3 axis);

		/**
		 * Change the local scale by an amount in the local x, y, z planes.
		 * This will propagate to Transforms that are relative to this one.
		 */
		void Scale(glm::vec3 xyz);

		void SetTranslate(glm::mat4 mat);
		glm::mat4 GetTranslate() const;
		void SetPosition(glm::vec3 pos);
		glm::vec3 GetPosition() const;

		glm::vec3 GetForward() const;

		void SetRotate(glm::mat4 mat);
		void SetRotate(glm::quat quat);
		glm::quat GetRotate() const;
		glm::mat4 GetRotateMatrix() const;

		void SetScale(glm::mat4 mat);
		glm::mat4 GetScale() const;

		bool ClearDirty();
	private:
		ecs::Entity::Id parent;
		ecs::EntityManager *manager;

		glm::mat4 translate;
		glm::mat4 scale;
		glm::quat rotate;

		bool dirty;
	};
}