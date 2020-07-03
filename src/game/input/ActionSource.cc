
#include "ActionSource.hh"
#include <Common.hh>
#include "InputManager.hh"
#include "ActionValue.hh"

namespace sp
{
	ActionSource::ActionSource(InputManager &inputManager) : input(&inputManager)
	{
        inputManager.AddActionSource(this);
	}

	ActionSource::~ActionSource()
    {
        if (input != nullptr)
        {
            input->RemoveActionSource(this);
        }
    }

    void ActionSource::BindAction(std::string action, std::string alias)
    {
        actionBindings[action] = alias;
    }
    
    void ActionSource::UnbindAction(std::string action)
    {
        actionBindings.erase(action);
    }
}
