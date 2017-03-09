#pragma once

#include <Ecs.hh>
#include "ecs/systems/HumanControlSystem.hh"
#include "Common.hh"
#include "core/CFunc.hh"

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

		void LoadScene(const string &name);
		void ReloadScene(const string &);

	private:
		Game *game;
		InputManager *input;
		ecs::HumanControlSystem humanControlSystem;
		shared_ptr<Scene> scene;
		ecs::Entity flashlight;
		bool flashlightFixed;
		float sunPos;

		CFuncCollection<string> funcs;
	};
}
