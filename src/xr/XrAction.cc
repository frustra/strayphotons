#include "xr/XrAction.hh"

#include <stdexcept>

using namespace sp;
using namespace xr;
using namespace std;

XrActionSet::XrActionSet(string name, string description) :
	actionSetName(name),
	actionSetDescription(description)
{
	// No other initialization needed
}

void XrActionSet::AddAction(shared_ptr<XrActionBase> action)
{
	registeredActions[action->GetName()] = action;
}

std::map<std::string, std::shared_ptr<XrActionBase>> &XrActionSet::GetActionMap()
{
	return registeredActions;
}

bool XrActionSet::GetBooleanActionValue(std::string actionName, std::string subpath)
{
	std::shared_ptr<XrActionBase> action = registeredActions[actionName];

	// This is actually fine. Just return a default value (false is a safe default value).
	if (!action->HasSubpath(subpath))
	{
		return false;
	}

	XrAction<XrActionType::Bool> *boolAction = dynamic_cast<XrAction<XrActionType::Bool>*>(action.get());

	if (boolAction)
	{
		return boolAction->actionData[subpath].value;
	}
	else
	{
		throw std::runtime_error("Does not appear to be a boolean action");
	}
}

bool XrActionSet::GetRisingEdgeActionValue(std::string actionName, std::string subpath)
{
	std::shared_ptr<XrActionBase> action = registeredActions[actionName];

	// This is actually fine. Just return a default value (false is a safe default value).
	if (!action->HasSubpath(subpath))
	{
		return false;
	}

	XrAction<XrActionType::Bool> *boolAction = dynamic_cast<XrAction<XrActionType::Bool>*>(action.get());

	if (boolAction)
	{
		return boolAction->actionData[subpath].value && (boolAction->actionData[subpath].value != boolAction->actionData[subpath].edge_val);
	}
	else
	{
		throw std::runtime_error("Does not appear to be a boolean action");
	}
}

bool XrActionSet::GetFallingEdgeActionValue(std::string actionName, std::string subpath)
{
	std::shared_ptr<XrActionBase> action = registeredActions[actionName];

	// This is actually fine. Just return a default value (false is a safe default value).
	if (!action->HasSubpath(subpath))
	{
		return false;
	}

	XrAction<XrActionType::Bool> *boolAction = dynamic_cast<XrAction<XrActionType::Bool>*>(action.get());

	if (boolAction)
	{
		return (!boolAction->actionData[subpath].value) && (boolAction->actionData[subpath].value != boolAction->actionData[subpath].edge_val);
	}
	else
	{
		throw std::runtime_error("Does not appear to be a boolean action");
	}
}

XrActionBase::XrActionBase(string name, XrActionType type) :
	actionName(name),
	actionType(type)
{
	// No other initialization needed
}

void XrActionBase::AddSuggestedBinding(string interactionProfile, string path)
{
	suggestedBindings[interactionProfile].push_back(path);
}
