#pragma once

#include "ecs/systems/AnimationSystem.hh"
#include "game/GameLogic.hh"
#include "game/gui/DebugGuiManager.hh"
#include "game/gui/MenuGuiManager.hh"
#include "graphics/GraphicsManager.hh"
#include "physx/PhysxManager.hh"

#include <Common.hh>
#include <chrono>
#include <ecs/Ecs.hh>
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

		std::unique_ptr<DebugGuiManager> debugGui = nullptr;
		std::unique_ptr<MenuGuiManager> menuGui = nullptr;

		GraphicsManager graphics;
		InputManager input;
		ecs::EntityManager entityManager;
		GameLogic logic;
		PhysxManager physics;
		ecs::AnimationSystem animation;

	private:
		chrono_clock::time_point lastFrameTime;
	};
} // namespace sp
