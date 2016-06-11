#pragma once

#include "graphics/GraphicsManager.hh"
#include "assets/AssetManager.hh"
#include "Common.hh"
#include "ecs/Ecs.hh"
#include "game/GameLogic.hh"
#include "game/InputManager.hh"

namespace sp
{
	class Game
	{
	public:
		Game();
		~Game();

		void Start();
		bool Frame();
		bool ShouldStop();

		GraphicsManager graphics;
		GameLogic logic;
		InputManager input;
		AssetManager assets;
		EntityManager entityManager;

	private:
		double lastFrameTime;
	};
}
