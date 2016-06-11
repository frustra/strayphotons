#include "ecs/systems/CameraSystem.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Camera.hh"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

namespace ECS
{
	CameraSystem::CameraSystem(sp::Game *game) : game(game)
	{
	}

	CameraSystem::~CameraSystem()
	{
	}

	/**
	* This camera's view/projection will be used when rendering
	*/
	void CameraSystem::SetActiveCamera(sp::Entity entity)
	{
		if (!entity.Has<Camera>() || !entity.Has<Transform>())
		{
			throw std::invalid_argument("entity needs to have camera and transform components");
		}
		activeCamera = entity;
	}

	glm::mat4 CameraSystem::GetActiveViewTransform()
	{
		if (!activeCamera.Valid() || !activeCamera.Has<Transform>())
		{
			throw std::runtime_error("active camera is no longer valid, wat do?");
		}

		auto *transform = activeCamera.Get<Transform>();
		return glm::inverse(transform->GetModelTransform(*activeCamera.GetManager()));
	}

	glm::mat4 CameraSystem::GetActiveProjectTransform()
	{
		// TODO-cs: parameterize
		return glm::perspective(glm::radians(60.0f), 1.778f, 0.1f, 256.0f);
	}

}