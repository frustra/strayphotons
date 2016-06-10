#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ecs/Ecs.hh"
#include "ecs/components/Transform.hh"

namespace ECS
{
	glm::mat4 Transform::GetModelTransform(sp::EntityManager &manager)
	{
		glm::mat4 model;

		if (this->relativeTo != sp::Entity::Id())
		{
			sp::Assert(
				manager.Has<Transform>(this->relativeTo),
				"cannot be relative to something that does not have a Transform"
			);

			model = manager.Get<Transform>(this->relativeTo)->GetModelTransform(manager);
		}

		return model * this->position * this->rotate * this->scale;
	}

	void Transform::SetRelativeTo(sp::Entity ent)
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
		this->position = glm::translate(this->position, xyz);
	}

	void Transform::SetTransform(glm::vec3 xyz)
	{
		this->position[0][3] = xyz.x;
		this->position[1][3] = xyz.y;
		this->position[2][3] = xyz.z;
	}

	void Transform::Scale(glm::vec3 xyz)
	{
		this->scale = glm::scale(this->scale, xyz);
	}
}