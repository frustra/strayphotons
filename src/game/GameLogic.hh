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
		InputManager *input;
		ecs::HumanControlSystem humanControlSystem;
		shared_ptr<Scene> scene;
		ecs::Entity flashlight;
		bool flashlightFixed;
	};
}
