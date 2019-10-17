#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/Console.hh"
#include "physx/PhysxUtils.hh"
#include "ecs/Components.hh"

#include "ecs/components/Barrier.hh"
#include "ecs/components/Interact.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Mirror.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/TriggerArea.hh"
#include "ecs/components/View.hh"
#include "ecs/components/VoxelInfo.hh"
#include "ecs/components/LightGun.hh"
#include "ecs/components/SlideDoor.hh"
#include "ecs/components/Animation.hh"
#include "ecs/components/SignalReceiver.hh"

#include <cxxopts.hpp>
#include <glm/glm.hpp>

namespace sp
{
	Game::Game(cxxopts::ParseResult &options) : options(options), menuGui(this), graphics(this), audio(this), logic(this), physics(), animation(entityManager)
	{
		// pre-register all of our component types so that errors do not arise if they
		// are queried for before an instance is ever created
		ecs::RegisterComponents(entityManager);
		// entityManager.RegisterComponentType<ecs::Transform>();
		// entityManager.RegisterComponentType<ecs::View>();
		entityManager.RegisterComponentType<ecs::Barrier>();
		entityManager.RegisterComponentType<ecs::HumanController>();
		entityManager.RegisterComponentType<ecs::InteractController>();
		entityManager.RegisterComponentType<ecs::Light>();
		entityManager.RegisterComponentType<ecs::LightSensor>();
		entityManager.RegisterComponentType<ecs::Physics>();
		entityManager.RegisterComponentType<ecs::Mirror>();
		entityManager.RegisterComponentType<ecs::Name>();
		entityManager.RegisterComponentType<ecs::Renderable>();
		entityManager.RegisterComponentType<ecs::TriggerArea>();
		entityManager.RegisterComponentType<ecs::VoxelInfo>();
		entityManager.RegisterComponentType<ecs::VoxelArea>();
		entityManager.RegisterComponentType<ecs::LightGun>();
		entityManager.RegisterComponentType<ecs::SlideDoor>();
		entityManager.RegisterComponentType<ecs::Animation>();
		entityManager.RegisterComponentType<ecs::SignalReceiver>();
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

		CFunc<void> cfExit("exit", "Quits the game", [&]()
		{
			triggeredExit = true;
		});

		entityManager.Subscribe<ecs::EntityDestruction>([&](ecs::Entity ent, const ecs::EntityDestruction & d)
		{
			if (ent.Has<ecs::Physics>())
			{
				auto phys = ent.Get<ecs::Physics>();
				auto rigidBody = phys->actor->is<physx::PxRigidDynamic>();
				if (rigidBody) physics.RemoveConstraints(rigidBody);
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
			debugGui.BindInput(input);
			menuGui.BindInput(input);
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
		GConsoleManager.Update();

		if (!logic.Frame(dt)) return false;
		if (!graphics.Frame()) return false;
		if (!audio.Frame()) return false;
		if (!physics.LogicFrame(entityManager)) return false;
		if (!animation.Frame(dt)) return false;

		lastFrameTime = frameTime;
		return true;
	}

	bool Game::ShouldStop()
	{
		return !graphics.HasActiveContext();
	}
}
