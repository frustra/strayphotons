#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <Ecs.hh>
#include "ecs/components/Transform.hh"

namespace ecs
{
	void Transform::SetParent(ecs::Entity ent)
	{
		if (!ent.Valid())
		{
			this->parent = ecs::Entity::Id();
			return;
		}

		if (!ent.Has<Transform>())
		{
			std::stringstream ss;
			ss << "Cannot set placement relative to " << ent
			   << " because it does not have a placement.";
			throw std::runtime_error(ss.str());
		}

		this->parent = ent.GetId();
		this->dirty = true;
	}

	bool Transform::HasParent()
	{
		return this->parent != ecs::Entity::Id();
	}

	glm::mat4 Transform::GetGlobalTransform() const
	{
		glm::mat4 model;

		if (this->parent != ecs::Entity::Id())
		{
			sp::Assert(
				manager->Has<Transform>(this->parent),
				"cannot be relative to something that does not have a Transform"
			);

			model = manager->Get<Transform>(this->parent)->GetGlobalTransform();
		}

		return model * this->translate * GetRotateMatrix() * this->scale;
	}

	glm::quat Transform::GetGlobalRotation() const
	{
		glm::quat model;

		if (this->parent != ecs::Entity::Id())
		{
			sp::Assert(
				manager->Has<Transform>(this->parent),
				"cannot be relative to something that does not have a Transform"
			);

			model = manager->Get<Transform>(this->parent)->GetGlobalRotation();
		}

		return model * this->rotate;
	}

	glm::vec3 Transform::GetGlobalPosition() const
	{
		return GetGlobalTransform() * glm::vec4(0, 0, 0, 1);
	}

	glm::vec3 Transform::GetGlobalForward() const
	{
		glm::vec3 forward = glm::vec3(0, 0, -1);
		return GetGlobalRotation() * forward;
	}

	void Transform::Translate(glm::vec3 xyz)
	{
		this->translate = glm::translate(this->translate, xyz);
		this->dirty = true;
	}

	void Transform::Rotate(float radians, glm::vec3 axis)
	{
		this->rotate = glm::rotate(this->rotate, radians, axis);
		this->dirty = true;
	}

	void Transform::Scale(glm::vec3 xyz)
	{
		this->scale = glm::scale(this->scale, xyz);
		this->dirty = true;
	}

	void Transform::SetTranslate(glm::mat4 mat)
	{
		this->translate = mat;
		this->dirty = true;
	}

	glm::mat4 Transform::GetTranslate() const
	{
		return this->translate;
	}

	void Transform::SetPosition(glm::vec3 pos)
	{
		this->translate = glm::column(this->translate, 3, glm::vec4(pos.x, pos.y, pos.z, 1.f));
		this->dirty = true;
	}

	glm::vec3 Transform::GetPosition() const
	{
		return this->translate * glm::vec4(0, 0, 0, 1);
	}

	glm::vec3 Transform::GetForward() const
	{
		glm::vec3 forward = glm::vec3(0, 0, -1);
		return GetRotate() * forward;
	}

	void Transform::SetRotate(glm::mat4 mat)
	{
		this->rotate = mat;
		this->dirty = true;
	}

	void Transform::SetRotate(glm::quat quat)
	{
		this->rotate = quat;
		this->dirty = true;
	}

	glm::quat Transform::GetRotate() const
	{
		return this->rotate;
	}

	glm::mat4 Transform::GetRotateMatrix() const
	{
		return glm::mat4_cast(rotate);
	}

	void Transform::SetScale(glm::vec3 xyz)
	{
		this->scale = glm::scale(glm::mat4(), xyz);
		this->dirty = true;
	}

	void Transform::SetScale(glm::mat4 mat)
	{
		this->scale = mat;
		this->dirty = true;
	}

	glm::mat4 Transform::GetScale() const
	{
		return this->scale;
	}

	bool Transform::ClearDirty()
	{
		bool tmp = this->dirty;
		this->dirty = false;
		return tmp;
	}
}
