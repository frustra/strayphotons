#include "core/Game.hh"
#include "core/Logging.hh"

#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Physics.hh"

namespace sp
{
	Game::Game() : graphics(this), physics()
	{
		Entity duck = entityManager.NewEntity();
		duck.Assign<ECS::Renderable>(assets.LoadModel("duck"));
		duck.Assign<ECS::Physics>(physics.CreateActor());

		entityManager.NewEntity().Assign<ECS::Renderable>(assets.LoadModel("box"));

		graphics.CreateContext();
	}

	Game::~Game()
	{
	}

	void Game::Start()
	{
		try
		{
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
		if (!graphics.Frame()) return false;
		physics.Frame();


		for (Entity ent : entityManager.EntitiesWith<ECS::Physics>())
		{
			auto comp = ent.Get<ECS::Physics>();
			physx::PxVec3 p = comp->actor->getGlobalPose().p;
			Debugf("(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ", " + std::to_string(p.z) + ")");
		}

		return true;
	}

	bool Game::ShouldStop()
	{
		return !graphics.HasActiveContext();
	}
}

