#pragma once

#include "physx/PhysxManager.hh"
#include "graphics/GraphicsManager.hh"
#include "assets/AssetManager.hh"
#include "Common.hh"
#include "ecs/Ecs.hh"
#include "game/GameLogic.hh"

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
		AssetManager assets;
		EntityManager entityManager;
		PhysxManager physics;

	private:
		double lastFrameTime;
	};
}
