#ifndef SP_GAME_H
#define SP_GAME_H

#include "graphics/GraphicsManager.hh"
#include "assets/AssetManager.hh"
#include "Shared.hh"

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

		sp::AssetManager assets;
		sp::GraphicsManager graphics;
	};
}

#endif

