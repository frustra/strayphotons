#ifndef SP_GAME_H
#define SP_GAME_H

#include "graphics/GraphicsManager.hh"
#include "assets/AssetManager.hh"
#include "Common.hh"
#include "ecs/Ecs.hh"

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

		AssetManager assets;
		GraphicsManager graphics;
		EntityManager entityManager;
	};
}

#endif

