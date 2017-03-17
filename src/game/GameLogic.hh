#pragma once

#include <Ecs.hh>
#include "ecs/systems/HumanControlSystem.hh"
#include "ecs/systems/LightGunSystem.hh"
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
		void PrintDebug(const string &);

		void OpenBarrier(const string &name);
		void CloseBarrier(const string &name);

		void OpenDoor(const string &name);
		void CloseDoor(const string &name);

	private:
		Game *game;
		InputManager *input;
		ecs::HumanControlSystem humanControlSystem;
		ecs::LightGunSystem lightGunSystem;
		shared_ptr<Scene> scene;
		ecs::Entity flashlight;
		float sunPos;

		CFuncCollection<GameLogic, string> funcs;
	};
}
