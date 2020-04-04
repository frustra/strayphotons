#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>

#include "xr/XrModel.hh"

namespace sp
{
namespace xr
{
	// Must match the OpenVR manifest files!
	static const char* GameActionSet = "/actions/main";

	static const char* GrabActionName = "/actions/main/in/grab";
	static const char* TeleportActionName = "/actions/main/in/teleport";
	static const char* LeftHandActionName = "/actions/main/in/LeftHand";
	static const char* RightHandActionName = "/actions/main/in/RightHand";

	static const char* SubpathLeftHand = "/user/hand/left";
	static const char* SubpathRightHand = "/user/hand/right";
	static const char* SubpathDefault = "/"; // TODO: correct?

	// Mimicking OpenXr spec
	enum XrActionType
	{
		Bool = 1,
		Float = 2,
		Vec2f = 3,
		Pose = 4,
	};

	template <XrActionType T>
	class XrActionDataType
	{
	};

	template <>
	class XrActionDataType<XrActionType::Bool>
	{
	public:
		XrActionDataType() : value(false), edge_val(false) {};
		XrActionDataType(bool init) : value(init), edge_val(false) {};
		void Reset()
		{
			value = false;    // Don't reset edge_val during reset!
		}
		bool value; // Actual current action value
		bool edge_val; // Previous action value
	};

	template <>
	class XrActionDataType<XrActionType::Float>
	{
		XrActionDataType() : value(0.0f) {};
		XrActionDataType(float init) : value(init) {};
		void Reset()
		{
			value = 0.0f;
		}
		float value;
	};

	template <>
	class XrActionDataType<XrActionType::Vec2f>
	{
		void Reset()
		{
			value = glm::vec2();
		}
		glm::vec2 value;
	};

	template <>
	class XrActionDataType<XrActionType::Pose>
	{
		void Reset()
		{
			value = glm::mat4();
		}
		glm::mat4 value;
	};

	class XrAction
	{
	public:
		XrAction(std::string name, XrActionType type);

		void Reset();

		void AddSuggestedBinding(std::string interactionProfile, std::string path);
		
		std::map<std::string, std::vector<std::string>> &GetSuggestedBindings();

		std::string GetName();

		XrActionType GetActionType();

		// Boolean action manipulation
		virtual bool GetBooleanActionValue(std::string subpath) = 0;

		// Returns true if the boolean action transitioned from false to true
		// during this update loop
		virtual bool GetRisingEdgeActionValue(std::string subpath) = 0;

		// Returns true if the boolean action transitioned from true to false
		// during this update loop
		virtual bool GetFallingEdgeActionValue(std::string subpath) = 0;

		virtual bool GetPoseActionValueForNextFrame(std::string subpath, glm::mat4 &pose) = 0;

	protected:
		// Map interaction profile name -> interaction paths
		std::map<std::string, std::vector<std::string>> suggestedBindings;
		std::string actionName;
		XrActionType actionType;		
	};

	class XrActionSet
	{
	public:
		XrActionSet(std::string setName, std::string description);

		virtual std::shared_ptr<XrAction> CreateAction(std::string name, XrActionType type) = 0;

		void AddAction(std::shared_ptr<XrAction> action);

		std::map<std::string, std::shared_ptr<XrAction>> &GetActionMap();

		std::shared_ptr<XrAction> GetAction(std::string name) { return registeredActions[name]; };

		virtual void Sync() = 0;

		virtual bool IsInputSourceConnected(std::string action) = 0;

		virtual std::shared_ptr<XrModel> GetInputSourceModel(std::string action) = 0;

	protected:
		std::string actionSetName;
		std::string actionSetDescription;
		std::map<std::string, std::shared_ptr<XrAction>> registeredActions;
	};

} // Namespace xr
} // Namespace sp