#ifndef SP_GAME_H
#define SP_GAME_H

#include "physx/PhysxManager.hh"
#include "graphics/GraphicsManager.hh"
#include "assets/AssetManager.hh"
#include "Common.hh"

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
		PhysxManager physics;
		shared_ptr<Model> duck;
	};
}

#endif

