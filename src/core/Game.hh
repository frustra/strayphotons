#pragma once

#include "Common.hh"
#include "ecs/systems/AnimationSystem.hh"
#include "game/GameLogic.hh"
#include "game/gui/DebugGuiManager.hh"
#include "game/gui/MenuGuiManager.hh"
#include "graphics/GraphicsManager.hh"
#include "physx/PhysxManager.hh"

#include <Ecs.hh>
#include <chrono>
#include <game/input/InputManager.hh>

namespace cxxopts {
	class ParseResult;
}

namespace sp {
	class Script;

	class Game {
	public:
		Game(cxxopts::ParseResult &options, Script *startupScript = nullptr);
		~Game();

		int Start();
		bool Frame();
		void PhysicsUpdate();
		bool ShouldStop();

		cxxopts::ParseResult &options;
		Script *startupScript = nullptr;

		// Order is important.
		DebugGuiManager debugGui;
		MenuGuiManager menuGui;
		GraphicsManager graphics;
		std::unique_ptr<InputManager> input;
		ecs::EntityManager entityManager;
		GameLogic logic;
		PhysxManager physics;
		ecs::AnimationSystem animation;

	private:
		std::chrono::high_resolution_clock::time_point lastFrameTime;
	};
} // namespace sp
