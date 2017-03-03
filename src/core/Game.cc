#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/Console.hh"

#include "ecs/components/Renderable.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Controller.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Mirror.hh"
#include "ecs/components/VoxelInfo.hh"
#include "ecs/components/Door.hh"

#include <cxxopts.hpp>
#include <glm/glm.hpp>

namespace sp
{
	Game::Game(cxxopts::Options &options) : options(options), graphics(this), audio(this), logic(this), physics()
	{
		// pre-register all of our component types so that errors do not arise if they
		// are queried for before an instance is ever created
		entityManager.RegisterComponentType<ecs::Renderable>();
		entityManager.RegisterComponentType<ecs::Transform>();
		entityManager.RegisterComponentType<ecs::Physics>();
		entityManager.RegisterComponentType<ecs::HumanController>();
		entityManager.RegisterComponentType<ecs::View>();
		entityManager.RegisterComponentType<ecs::Light>();
		entityManager.RegisterComponentType<ecs::LightSensor>();
		entityManager.RegisterComponentType<ecs::Mirror>();
		entityManager.RegisterComponentType<ecs::VoxelInfo>();
		entityManager.RegisterComponentType<ecs::Door>();
	}

	Game::~Game()
	{
	}

	void Game::Start()
	{
		if (options.count("cvar"))
		{
			for (auto cvarline : options["cvar"].as<vector<string>>())
			{
				GConsoleManager.ParseAndExecute(cvarline);
			}
		}

		bool triggeredExit = false;

		CFunc cfExit("exit", "Quits the game", [&](const string & s)
		{
			triggeredExit = true;
		});

		entityManager.Subscribe<ecs::EntityDestruction>([&](ecs::Entity ent, const ecs::EntityDestruction & d)
		{
			if (ent.Has<ecs::Physics>())
			{
				auto phys = ent.Get<ecs::Physics>();
				physics.RemoveActor(phys->actor);
				phys->model = nullptr;
			}
			if (ent.Has<ecs::HumanController>())
			{
				auto controller = ent.Get<ecs::HumanController>();
				physics.RemoveController(controller->pxController);
			}
		});

		try
		{
			// audio.LoadProjectFiles();
			logic.Init();
			graphics.CreateContext();
			graphics.BindContextInputCallbacks(input);
			gui.BindInput(input);
			lastFrameTime = glfwGetTime();

			while (!triggeredExit)
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
		double dt = frameTime - lastFrameTime;

		input.Checkpoint();
		if (input.IsDown(GLFW_KEY_ESCAPE))
		{
			return false;
		}

		GConsoleManager.Update();

		if (!logic.Frame(dt)) return false;
		if (!graphics.Frame()) return false;
		if (!audio.Frame()) return false;
		PhysicsUpdate();

		lastFrameTime = frameTime;
		return true;
	}

	void Game::PhysicsUpdate()
	{
		{
			// Sync transforms to physx
			bool gotLock = false;

			for (ecs::Entity ent : entityManager.EntitiesWith<ecs::Physics>())
			{
				auto ph = ent.Get<ecs::Physics>();

				if (ph->needsTransformSync)
				{
					if (!gotLock)
					{
						physics.Lock();
						gotLock = true;
					}

					auto transform = ent.Get<ecs::Transform>();
					auto mat = transform->GetModelTransform(entityManager);
					auto pxMat = *(physx::PxMat44 *)(glm::value_ptr(mat));

					physx::PxTransform newPose(pxMat);
					ph->actor->setGlobalPose(newPose);
					ph->needsTransformSync = false;
				}
			}

			if (gotLock)
				physics.Unlock();
		}

		{
			// Sync transforms from physx
			physics.ReadLock();

			for (ecs::Entity ent : entityManager.EntitiesWith<ecs::Physics>())
			{
				auto physics = ent.Get<ecs::Physics>();
				auto pxMat = (physx::PxMat44)(physics->actor->getGlobalPose());
				auto mat = *((glm::mat4 *) pxMat.front());

				auto transform = ent.Get<ecs::Transform>();
				transform->SetTransform(mat);
			}

			physics.ReadUnlock();
		}
	}

	bool Game::ShouldStop()
	{
		return !graphics.HasActiveContext();
	}
}
