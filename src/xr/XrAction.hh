#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>

namespace sp
{

	namespace xr
	{

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

		class XrActionBase
		{
		public:
			XrActionBase(std::string name, XrActionType type);

			void AddSuggestedBinding(std::string interactionProfile, std::string path);
			std::map<std::string, std::vector<std::string>> &GetSuggestedBindings()
			{
				return suggestedBindings;
			};

			std::string GetName()
			{
				return actionName;
			};
			XrActionType GetActionType()
			{
				return actionType;
			};

			virtual void Reset(std::string subpath) = 0;

			virtual void AddSubactionPath(std::string subactionPath) = 0;

			virtual bool HasSubpath(std::string subpath) = 0;

		protected:
			// Map interaction profile name -> interaction paths
			std::map<std::string, std::vector<std::string>> suggestedBindings;
			std::string actionName;
			XrActionType actionType;
		};

		template <XrActionType T>
		class XrAction : public XrActionBase
		{
		public:
			XrAction(std::string name) : XrActionBase(name, T)
			{
				AddSubactionPath("");
			};

			void Reset(std::string subpath)
			{
				actionData[subpath].Reset();
			};

			void AddSubactionPath(std::string subactionPath)
			{
				actionData[subactionPath] = XrActionDataType<T>();
			};

			bool HasSubpath(std::string subpath)
			{
				return (actionData.count(subpath) != 0);
			};

			std::map<std::string, XrActionDataType<T>> actionData;
		};

		class XrActionSet
		{
		public:
			XrActionSet(std::string setName, std::string description);

			void AddAction(std::shared_ptr<XrActionBase> action);

			std::map<std::string, std::shared_ptr<XrActionBase>> &GetActionMap();

			// Boolean action manipulation
			bool GetBooleanActionValue(std::string actionName, std::string subpath = "");

			// Returns true if the boolean action transitioned from false to true
			// during this update loop
			bool GetRisingEdgeActionValue(std::string actionName, std::string subpath = "");

			// Returns true if the boolean action transitioned from true to false
			// during this update loop
			bool GetFallingEdgeActionValue(std::string actionName, std::string subpath = "");

		protected:
			std::string actionSetName;
			std::string actionSetDescription;
			std::map<std::string, std::shared_ptr<XrActionBase>> registeredActions;
		};

	} // Namespace xr
} // Namespace sp