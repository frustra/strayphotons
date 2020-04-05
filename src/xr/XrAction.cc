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

void XrActionSet::AddAction(shared_ptr<XrAction> action)
{
	registeredActions[action->GetName()] = action;
}

std::map<std::string, std::shared_ptr<XrAction>> &XrActionSet::GetActionMap()
{
	return registeredActions;
}

XrAction::XrAction(string name, XrActionType type) : 
	actionName(name), 
	actionType(type)
{
};

void XrAction::AddSuggestedBinding(string interactionProfile, string path)
{
	suggestedBindings[interactionProfile].push_back(path);
}

map<string, vector<string>> &XrAction::GetSuggestedBindings()
{
	return suggestedBindings;
};

string XrAction::GetName()
{
	return actionName;
};

XrActionType XrAction::GetActionType()
{
	return actionType;
};