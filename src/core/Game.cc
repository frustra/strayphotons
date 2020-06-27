#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/Console.hh"
#include "physx/PhysxUtils.hh"
#include "ecs/Components.hh"

#include "ecs/components/Physics.hh"
#include <game/input/GlfwInputManager.hh>
#include <assets/Script.hh>

#include <cxxopts.hpp>
#include <glm/glm.hpp>

namespace sp
{
	Game::Game(cxxopts::ParseResult & options, Script *startupScript) : options(options), startupScript(startupScript), menuGui(this), graphics(this), logic(this), physics(), animation(entityManager)
	{
		// pre-register all of our component types so that errors do not arise if they
		// are queried for before an instance is ever created
		ecs::RegisterComponents(entityManager);
	}

	Game::~Game()
	{
	}

	int Game::Start()
	{
		bool triggeredExit = false;
		int exitCode = 0;

		CFunc<int> cfExit("exit", "Quits the game", [&](int arg)
		{
			triggeredExit = true;
			exitCode = arg;
		});

		if (options.count("cvar"))
		{
			for (auto cvarline : options["cvar"].as<vector<string>>())
			{
				GConsoleManager.ParseAndExecute(cvarline);
			}
		}

		entityManager.Subscribe<ecs::EntityDestruction>([&](ecs::Entity ent, const ecs::EntityDestruction & d)
		{
			if (ent.Has<ecs::Physics>())
			{
				auto phys = ent.Get<ecs::Physics>();
				if (phys->actor)
				{
					auto rigidBody = phys->actor->is<physx::PxRigidDynamic>();
					if (rigidBody) physics.RemoveConstraints(rigidBody);
					physics.RemoveActor(phys->actor);
				}
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
			input = std::make_unique<GlfwInputManager>();
			auto glfwInput = (GlfwInputManager *) input.get();

			graphics.CreateContext();
			logic.Init(input.get(), startupScript);
			graphics.BindContextInputCallbacks(glfwInput);
			debugGui.BindInput(glfwInput);
			menuGui.BindInput(glfwInput);
			lastFrameTime = glfwGetTime();

			while (!triggeredExit)
			{
				if (ShouldStop()) break;
				if (!Frame()) break;
			}
			return exitCode;
		}
		catch (char const *err)
		{
			Errorf(err);
			return 1;
		}
	}

	bool Game::Frame()
	{
		double frameTime = glfwGetTime();
		double dt = frameTime - lastFrameTime;

		input->BeginFrame();

		GConsoleManager.Update(startupScript);

		if (!logic.Frame(dt)) return false;
		if (!graphics.Frame()) return false;
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
