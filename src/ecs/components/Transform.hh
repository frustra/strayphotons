#pragma once

#include "Common.hh"

#include <ecs/Components.hh>
#include <ecs/NamedEntity.hh>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ecs {
	class Transform {
	public:
		Transform() : dirty(false) {}
		Transform(glm::vec3 pos, glm::quat orientation = glm::identity<glm::quat>())
			: rotate(orientation), dirty(false) {
			SetPosition(pos);
		}

		void SetParent(ecs::Entity ent);
		void SetParent(ecs::NamedEntity ent);
		bool HasParent(EntityManager &em);

		/**
		 * Return the transformation matrix including all parent transforms.
		 */
		glm::mat4 GetGlobalTransform(EntityManager &em);

		/**
		 * Returns the same as GetGlobalTransform() but only includes rotation components.
		 */
		glm::quat GetGlobalRotation(EntityManager &em);

		/**
		 * Get position including any transforms this is relative to
		 */
		glm::vec3 GetGlobalPosition(EntityManager &em);

		/**
		 * Get forward vector including any transforms this is relative to
		 */
		glm::vec3 GetGlobalForward(EntityManager &em);

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

		glm::vec3 GetUp() const;
		glm::vec3 GetForward() const;
		glm::vec3 GetLeft() const;
		glm::vec3 GetRight() const;

		void SetRotate(glm::mat4 mat);
		void SetRotate(glm::quat quat);
		glm::quat GetRotate() const;
		glm::mat4 GetRotateMatrix() const;

		void SetScale(glm::mat4 mat);
		void SetScale(glm::vec3 xyz);
		glm::mat4 GetScale() const;
		glm::vec3 GetScaleVec() const;

		bool ClearDirty();

	private:
		ecs::NamedEntity parent;

		glm::mat4 translate = glm::identity<glm::mat4>();
		glm::mat4 scale = glm::identity<glm::mat4>();
		glm::quat rotate = glm::identity<glm::quat>();

		glm::mat4 cachedTransform = glm::identity<glm::mat4>();
		uint32 changeCount = 1;
		uint32 cacheCount = 0;
		uint32 parentCacheCount = 0;

		bool dirty;
	};

	static Component<Transform> ComponentTransform("transform");

	template<>
	bool Component<Transform>::LoadEntity(Entity &dst, picojson::value &src);
} // namespace ecs
