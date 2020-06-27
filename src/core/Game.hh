#pragma once

#include "physx/PhysxManager.hh"
#include "graphics/GraphicsManager.hh"
#include "Common.hh"
#include "game/GameLogic.hh"
#include "ecs/systems/AnimationSystem.hh"
#include <game/input/InputManager.hh>
#include "game/gui/DebugGuiManager.hh"
#include "game/gui/MenuGuiManager.hh"

#include <Ecs.hh>

namespace cxxopts
{
	class ParseResult;
}

namespace sp
{
	class Script;

	class Game
	{
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
		double lastFrameTime;
	};
}
