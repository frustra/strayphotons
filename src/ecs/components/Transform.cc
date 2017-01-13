#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <Ecs.hh>
#include "ecs/components/Transform.hh"

namespace ecs
{
	glm::mat4 Transform::GetModelTransform(ecs::EntityManager &manager)
	{
		glm::mat4 model;

		if (this->relativeTo != ecs::Entity::Id())
		{
			sp::Assert(
				manager.Has<Transform>(this->relativeTo),
				"cannot be relative to something that does not have a Transform"
			);

			model = manager.Get<Transform>(this->relativeTo)->GetModelTransform(manager);
		}

		return model * this->translate * GetRotateMatrix() * this->scale;
	}

	void Transform::SetRelativeTo(ecs::Entity ent)
	{
		if (!ent.Has<Transform>())
		{
			std::stringstream ss;
			ss << "Cannot set placement relative to " << ent
			   << " because it does not have a placement.";
			throw std::runtime_error(ss.str());
		}

		this->relativeTo = ent.GetId();
	}

	void Transform::Rotate(float radians, glm::vec3 axis)
	{
		this->rotate = glm::rotate(this->rotate, radians, axis);
	}

	void Transform::Translate(glm::vec3 xyz)
	{
		this->translate = glm::translate(this->translate, xyz);
	}

	void Transform::SetTransform(glm::mat4 mat)
	{
		this->translate = mat;
	}

	void Transform::SetPosition(glm::vec3 pos)
	{
		this->translate = glm::column(this->translate, 3, glm::vec4(pos.x, pos.y, pos.z, 1.f));
	}

	glm::vec3 Transform::GetPosition()
	{
		return this->translate*glm::vec4(0,0,0,1);
	}

	void Transform::Scale(glm::vec3 xyz)
	{
		this->scale = glm::scale(this->scale, xyz);
	}

	glm::mat4 Transform::GetRotateMatrix()
	{
		return glm::mat4_cast(rotate);
	}
}