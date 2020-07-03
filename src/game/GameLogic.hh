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
	class Script;
	class InputManager;

	class GameLogic
	{
	public:
		GameLogic(Game *game);
		~GameLogic();

		void Init(Script *startupScript = nullptr);
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
		ecs::Entity UpdateXrActionEntity(xr::XrActionPtr action, bool active);

		void UpdateSkeletonDebugHand(xr::XrActionPtr action, glm::mat4 xrObjectPos, std::vector<xr::XrBoneData>& boneData, bool active);

		void ComputeBonePositions(std::vector<xr::XrBoneData> &boneData, std::vector<glm::mat4> &output);

		ecs::Entity GetLaserPointer();

	private:
		Game *game;
		InputManager *input = nullptr;
		ecs::HumanControlSystem humanControlSystem;
		ecs::LightGunSystem lightGunSystem;
		ecs::DoorSystem doorSystem;
		shared_ptr<Scene> scene;
		ecs::Entity flashlight;
		float sunPos;

		xr::XrSystemPtr xrSystem;
		xr::XrActionSetPtr gameActionSet;

		// Actions we use in game navigation
		xr::XrActionPtr teleportAction;
		xr::XrActionPtr grabAction;

		// Actions for the raw input device pose
		xr::XrActionPtr leftHandAction;
		xr::XrActionPtr rightHandAction;
		
		// Actions for the skeleton pose
		xr::XrActionPtr leftHandSkeletonAction;
		xr::XrActionPtr rightHandSkeletonAction;

		bool xrTeleported = false;
		bool xrGrabbed = false;

		CFuncCollection funcs;
	};
}
