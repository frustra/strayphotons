#include "core/Game.hh"
#include "core/Logging.hh"

#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Physics.hh"

#include "ecs/components/Transform.hh"

#include <glm/glm.hpp>

namespace sp
{
	Game::Game() : graphics(this), logic(this), physics()
	{
		// pre-register all of our component types so that errors do not arrise if they
		// are queried for before an instance is ever created
		entityManager.RegisterComponentType<ECS::Renderable>();
		entityManager.RegisterComponentType<ECS::Transform>();
		entityManager.RegisterComponentType<ECS::Physics>();
	}

	Game::~Game()
	{
	}

	void Game::Start()
	{
		try
		{
			logic.Init();
			graphics.CreateContext();
			this->lastFrameTime = glfwGetTime();

			while (true)
			{
				if (ShouldStop()) break;
				if (!Frame()) break;
			}
		}
		catch (char const *err)
		{
			Errorf(err);
		}
	}

	bool Game::Frame()
	{
		double frameTime = glfwGetTime();
		double dt = this->lastFrameTime - frameTime;

		if (!logic.Frame(dt)) return false;
		if (!graphics.Frame()) return false;
		physics.Frame(-dt);


		for (Entity ent : entityManager.EntitiesWith<ECS::Physics>())
		{
			auto physics = ent.Get<ECS::Physics>();
			physx::PxTransform pxT = physics->actor->getGlobalPose();
			physx::PxVec3 p = pxT.p;
			physx::PxQuat q = pxT.q;
			auto transform = ent.Get<ECS::Transform>();

			glm::vec3 glmV;
			glmV.x = p.x;
			glmV.y = p.y;
			glmV.z = p.z;

			transform->SetTransform(glmV);
			//TODO: Speak to Cory about getting transform to take in a quat
			//transform->Rotate((glm::quat) q);
		}

		this->lastFrameTime = frameTime;
		return true;
	}

	bool Game::ShouldStop()
	{
		return !graphics.HasActiveContext();
	}
}

