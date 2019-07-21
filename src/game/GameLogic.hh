#pragma once

#include <Ecs.hh>
#include "ecs/systems/HumanControlSystem.hh"
#include "ecs/systems/LightGunSystem.hh"
#include "ecs/systems/DoorSystem.hh"
#include "Common.hh"
#include "core/CFunc.hh"

namespace vr
{
	class IVRSystem;
}

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

		void LoadScene(string name);
		void ReloadScene(string arg);
		void PrintDebug();

		void OpenBarrier(string name);
		void CloseBarrier(string name);

		void OpenDoor(string name);
		void CloseDoor(string name);

		ecs::Entity GetPlayer();

	private:
		Game *game;
		InputManager *input;
		ecs::HumanControlSystem humanControlSystem;
		ecs::LightGunSystem lightGunSystem;
		ecs::DoorSystem doorSystem;
		shared_ptr<Scene> scene;
		ecs::Entity flashlight;
		float sunPos;

#ifdef ENABLE_VR
		vr::IVRSystem *vrSystem = nullptr;

		bool vrTriggerPressed = false;
		bool vrTouchpadPressed = false;

		ecs::Entity eyeEntity[2];
		glm::mat4 eyePos[2];
#endif

		CFuncCollection funcs;
	};
}
