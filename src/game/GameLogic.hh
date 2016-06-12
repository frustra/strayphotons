#pragma once

#include "ecs/systems/HumanControlSystem.hh"
#include "Common.hh"

namespace sp
{
	class Game;
	class Scene;

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
		shared_ptr<Scene> scene;
	};
}
