#include "core/Game.hh"

#include "core/Console.hh"
#include "core/Logging.hh"
#include "ecs/Components.hh"
#include "ecs/components/Physics.hh"
#include "physx/PhysxUtils.hh"

#include <Common.hh>
#include <assets/Script.hh>
#include <cxxopts.hpp>
#include <glm/glm.hpp>

namespace sp {
	Game::Game(cxxopts::ParseResult &options, Script *startupScript)
		: options(options), startupScript(startupScript), graphics(this), logic(this), physics(),
		  animation(entityManager) {
		// pre-register all of our component types so that errors do not arise if they
		// are queried for before an instance is ever created
		ecs::RegisterComponents(entityManager);

		debugGui = std::make_unique<DebugGuiManager>(this);
		menuGui = std::make_unique<MenuGuiManager>(this);
	}

	Game::~Game() {}

	int Game::Start() {
		bool triggeredExit = false;
		int exitCode = 0;

		CFunc<int> cfExit("exit", "Quits the game", [&](int arg) {
			triggeredExit = true;
			exitCode = arg;
		});

		if (options.count("cvar")) {
			for (auto cvarline : options["cvar"].as<vector<string>>()) {
				GetConsoleManager().ParseAndExecute(cvarline);
			}
		}

		entityManager.Subscribe<ecs::EntityDestruction>([&](ecs::Entity ent, const ecs::EntityDestruction &d) {
			if (ent.Has<ecs::Physics>()) {
				auto phys = ent.Get<ecs::Physics>();
				if (phys->actor) {
					auto rigidBody = phys->actor->is<physx::PxRigidDynamic>();
					if (rigidBody)
						physics.RemoveConstraints(rigidBody);
					physics.RemoveActor(phys->actor);
				}
				phys->model = nullptr;
			}
			if (ent.Has<ecs::HumanController>()) {
				auto controller = ent.Get<ecs::HumanController>();
				physics.RemoveController(controller->pxController);
			}
		});

		try {
			graphics.CreateContext();
			logic.Init(startupScript);

			lastFrameTime = chrono_clock::now();

			while (!triggeredExit) {
				if (ShouldStop())
					break;
				if (!Frame())
					break;
			}
			return exitCode;
		} catch (char const *err) {
			Errorf(err);
			return 1;
		}
	}

	bool Game::Frame() {
		input.BeginFrame();
		GetConsoleManager().Update(startupScript);

		auto frameTime = chrono_clock::now();
		double dt = (double)(frameTime - lastFrameTime).count();
		dt /= chrono_clock::duration(std::chrono::seconds(1)).count();

		if (!logic.Frame(dt))
			return false;
		if (!graphics.Frame())
			return false;
		if (!physics.LogicFrame(entityManager))
			return false;
		if (!animation.Frame(dt))
			return false;

		lastFrameTime = frameTime;
		return true;
	}

	bool Game::ShouldStop() {
		return !graphics.HasActiveContext();
	}
} // namespace sp
