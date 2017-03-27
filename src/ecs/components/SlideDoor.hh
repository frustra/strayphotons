#pragma once

#include <Ecs.hh>
#include <glm/glm.hpp>

namespace ecs
{
	class SlideDoor
	{
	public:
		enum State
		{
			CLOSED,
			OPENED,
			OPENING,
			CLOSING
		};

		SlideDoor::State GetState();
		void Close();
		void Open();
		void ValidateDoor() const;
		void ApplyParams();

		Entity left;
		Entity right;

		float width = 1.0f;
		float openTime = 0.5f;
		glm::vec3 forward = glm::vec3(0, 0, -1);
	};
}