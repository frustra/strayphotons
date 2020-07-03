#pragma once

#include "xr/XrModel.hh"

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sp {
	namespace xr {
		// Must match the OpenVR manifest files!
		static const char *GameActionSet = "/actions/main";

		static const char *GrabActionName = "/actions/main/in/grab";
		static const char *TeleportActionName = "/actions/main/in/teleport";
		static const char *LeftHandActionName = "/actions/main/in/LeftHand";
		static const char *RightHandActionName = "/actions/main/in/RightHand";

		static const char *LeftHandSkeletonActionName = "/actions/main/in/lefthand_anim";
		static const char *RightHandSkeletonActionName = "/actions/main/in/righthand_anim";

		static const char *SubpathLeftHand = "/user/hand/left";
		static const char *SubpathRightHand = "/user/hand/right";
		static const char *SubpathUser = "/user";
		static const char *SubpathNone = "";

		// Mimicking OpenXr spec
		enum XrActionType {
			Bool = 1,
			Float = 2,
			Vec2f = 3,
			Pose = 4,
			Skeleton = 5,
		};

		struct XrBoneData {
			glm::vec3 pos;
			glm::quat rot;
			glm::mat4 inverseBindPose;
		};

		class XrAction {
		public:
			// Create a new XR Action with a given name (this should be a valid XR action path) and
			// of a given XrActionType.
			XrAction(std::string name, XrActionType type);

			// Add a suggested binding for a particular controller interaction profile to this action.
			// NOTE: not all XR runtimes will respect this suggestion.
			void AddSuggestedBinding(std::string interactionProfile, std::string path);

			// Get the set of suggested bindings for a particular action, across all interaction profiles.
			std::map<std::string, std::vector<std::string>> &GetSuggestedBindings();

			// Get the action's name, which is a valid XR action path
			std::string GetName();

			// Return's the action's type
			XrActionType GetActionType();

			// Boolean action manipulation
			virtual bool GetBooleanActionValue(std::string subpath, bool &value) = 0;

			// Returns true if the boolean action transitioned from false to true
			// during this update loop
			virtual bool GetRisingEdgeActionValue(std::string subpath, bool &value) = 0;

			// Returns true if the boolean action transitioned from true to false
			// during this update loop
			virtual bool GetFallingEdgeActionValue(std::string subpath, bool &value) = 0;

			// Returns a glm::mat4 representing the pose for this action during the next frame.
			// This should be accessed during the Game::Frame() function. The pose returned by this
			// function is intended to be "visually correct" when rendered _after_ the next call to.
			// WaitGetPoses().
			virtual bool GetPoseActionValueForNextFrame(std::string subpath, glm::mat4 &pose) = 0;

			// Return the skeleton pose, which is a vector of bones that the runtime has returned to represent the
			// skeleton. The runtime must return a compatible skinnable XrModel from GetInputSourceModel() if it
			// provides valid bone data.
			virtual bool GetSkeletonActionValue(std::vector<XrBoneData> &bones, bool withController) = 0;

			// Get a Model representing this action's input source. Note that this only works for Pose and Skeleton
			// action types.
			// TODO: can this directly return some kind of std::future, since it has to run async in some cases? #41
			virtual std::shared_ptr<XrModel> GetInputSourceModel() = 0;

		protected:
			// Map interaction profile name -> interaction paths
			std::map<std::string, std::vector<std::string>> suggestedBindings;
			std::string actionName;
			XrActionType actionType;
		};

		class XrActionSet {
		public:
			// Create a new action set with a given name and description
			XrActionSet(std::string setName, std::string description);

			// Create an action as part of this action set
			virtual std::shared_ptr<XrAction> CreateAction(std::string name, XrActionType type) = 0;

			// Add an action created elsewhere to this action set
			void AddAction(std::shared_ptr<XrAction> action);

			// Get the complete mapping of action names to action handles
			std::map<std::string, std::shared_ptr<XrAction>> &GetActionMap();

			// Get a specific action given an action name
			std::shared_ptr<XrAction> GetAction(std::string name) {
				return registeredActions[name];
			};

			virtual void Sync() = 0;

		protected:
			std::string actionSetName;
			std::string actionSetDescription;
			std::map<std::string, std::shared_ptr<XrAction>> registeredActions;
		};

		typedef std::shared_ptr<XrAction> XrActionPtr;
		typedef std::shared_ptr<XrActionSet> XrActionSetPtr;

	} // Namespace xr
} // Namespace sp