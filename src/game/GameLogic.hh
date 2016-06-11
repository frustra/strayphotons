#pragma once

#include "ecs/systems/HumanControlSystem.hh"
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
		ECS::HumanControlSystem humanControlSystem;

		Entity duck;
		Entity box;
		Entity sponza;
	};
}
