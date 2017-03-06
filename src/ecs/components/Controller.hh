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
		MOVE_UP,
		MOVE_DOWN,
		MOVE_JUMP,
		INTERACT
	};

	// Will move these to HumanControlSystem
	static float CONTROLLER_GRAVITY = 9.81f;
	static float CONTROLLER_JUMP = 8.0f;
	static float CONTROLLER_STEP = 0.1f;
	static float CONTROLLER_AIR_STRAFE = 0.1f;
	static float CONTROLLER_ACCELERATION = 50.0f;

	struct HumanController
	{
		HumanController()
		{
			lastVelocity = glm::vec3();
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
		bool grounded;

		glm::vec3 lastVelocity;
		float upVelocity = 0.f;
	};

}

