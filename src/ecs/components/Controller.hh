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
		MOVE_SPRINT,
		INTERACT
	};

	// Units in meters
	const float PLAYER_HEIGHT = 1.7f;
	const float PLAYER_RADIUS = 0.2f;
	const float PLAYER_STEP_HEIGHT = 0.3f;
	const float PLAYER_SWEEP_DISTANCE = 0.4f; // Distance to check if on ground

	const float PLAYER_GRAVITY = 9.81f;
	const float PLAYER_JUMP_VELOCITY = 5.0f;
	const float PLAYER_AIR_STRAFE = 0.8f; // Movement scaler for acceleration in air
	const float PLAYER_PUSH_FORCE = 0.3f;

	struct HumanController
	{
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

		bool crouched = false;
		bool onGround = false;
		glm::vec3 velocity = glm::vec3(0);
	};

}

