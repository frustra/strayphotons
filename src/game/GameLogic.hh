#pragma once

#include <Ecs.hh>
#include "ecs/systems/HumanControlSystem.hh"
#include "ecs/systems/LightGunSystem.hh"
#include "ecs/systems/DoorSystem.hh"
#include "Common.hh"
#include "core/CFunc.hh"
#include "xr/XrSystem.hh"
#include "xr/XrAction.hh"

namespace sp
{
	class Game;
	class Scene;
	class InputManager;

	class GameLogic
	{
	public:
		GameLogic(Game *game);
		~GameLogic();

		void Init(InputManager *inputManager);
		void HandleInput();
		bool Frame(double dtSinceLastFrame);

		void LoadScene(string name);
		void ReloadScene(string arg);
		void PrintDebug();

		void OpenBarrier(string name);
		void CloseBarrier(string name);

		void OpenDoor(string name);
		void CloseDoor(string name);

		ecs::Entity GetPlayer();

		shared_ptr<xr::XrSystem> GetXrSystem();
		void InitXrActions();
		ecs::Entity ValidateAndLoadTrackedObject(sp::xr::TrackedObjectHandle &handle);

	private:
		Game *game;
		InputManager *input = nullptr;
		ecs::HumanControlSystem humanControlSystem;
		ecs::LightGunSystem lightGunSystem;
		ecs::DoorSystem doorSystem;
		shared_ptr<Scene> scene;
		ecs::Entity flashlight;
		float sunPos;

		shared_ptr<xr::XrSystem> xrSystem;
		xr::XrActionSet gameXrActions;

		bool xrTeleported = false;
		bool xrGrabbed = false;

		CFuncCollection funcs;
	};
}
