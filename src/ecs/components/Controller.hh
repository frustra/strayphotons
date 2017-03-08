#pragma once

#include "Common.hh"

#include <unordered_map>
#include <vector>
#include <functional>

#include <characterkinematic/PxCapsuleController.h>

#include <glm/glm.hpp>

namespace ecs
{
	enum class ControlAction
	{
		MOVE_FORWARD,
		MOVE_BACKWARD,
		MOVE_LEFT,
		MOVE_RIGHT,
		MOVE_JUMP,
		MOVE_CROUCH,
		INTERACT
	};

	const float CONTROLLER_SWEEP_DISTANCE = 0.4f;
	const float CONTROLLER_GRAVITY = 9.81f;
	const float CONTROLLER_JUMP = 2.5f;
	const float CONTROLLER_STEP = 0.1f;
	const float CONTROLLER_AIR_STRAFE = 0.3f;

	struct HumanController
	{
		HumanController()
		{
			lastGroundVelocity = glm::vec3();
		}

		// map each action to a vector of glfw keys that could trigger it
		// (see InputManager.hh for converting a mouse button to one of these ints)
		std::unordered_map<ControlAction, std::vector<int>, sp::EnumHash> inputMap;

		// overrides ecs::Placement::rotate for an FPS-style orientation
		// until I figure out how to actually use quaternions
		float pitch;
		float yaw;
		float roll;

		// pxCapsuleController handles movement and physx simulation
		physx::PxController *pxController;

		// Custom physics to handle gravity
		bool jumping = true;

		glm::vec3 lastGroundVelocity;
		float upVelocity = 0.f;
	};

}

