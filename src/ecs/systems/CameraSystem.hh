#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Camera.hh"
#include "ecs/components/Transform.hh"

#include <glm/glm.hpp>

namespace sp
{
	class Game;
}

namespace ECS
{
	class CameraSystem
	{
	public:
		CameraSystem(sp::Game *game);
		~CameraSystem();

		/**
		 * This camera's view/projection will be used when rendering
		 */
		void SetActiveCamera(sp::Entity entity);

		glm::mat4 GetActiveViewTransform();
		glm::mat4 GetActiveProjectTransform();

	private:
		sp::Game* game;
		sp::Entity activeCamera;
	};
}