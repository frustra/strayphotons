#pragma once

#include "Common.hh"

namespace sp
{
	class Game;

	class GameLogic
	{
	public:
		GameLogic(Game *game);
		~GameLogic();

		void Init();
		bool Frame(double dtSinceLastFrame);
	private:
		Game *game;
		Entity duck;
		Entity box;
		Entity sponza;
	};
}
