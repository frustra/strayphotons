#include <graphics/GraphicsManager.hh>

#include "GlfwInputManager.hh"
#include "BindingNames.hh"
#include <Common.hh>

#include <algorithm>
#include <stdexcept>
#include <core/Logging.hh>
#include <core/Console.hh>

namespace sp
{
	InputManager::InputManager()
	{
		funcs.Register(this, "bind", "Bind a key to a command", &InputManager::BindKey);
	}

	InputManager::~InputManager() {}

	bool InputManager::FocusLocked(FocusLevel priority) const
	{
		return focusLocked.size() > 0 && focusLocked.back() > priority;
	}

	bool InputManager::LockFocus(bool locked, FocusLevel priority)
	{
		if (locked)
		{
			if (focusLocked.empty() || focusLocked.back() < priority)
			{
				focusLocked.push_back(priority);
				return true;
			}
		}
		else
		{
			if (!focusLocked.empty() && focusLocked.back() == priority)
			{
				focusLocked.pop_back();
				return true;
			}
		}

		return false;
	}

	void InputManager::BindKey(string args)
	{
		std::stringstream stream(args);
		string keyName;
		stream >> keyName;

		auto it = UserBindingNames.find(to_upper_copy(keyName));
		if (it != UserBindingNames.end())
		{
			string command;
			std::getline(stream, command);
			trim(command);

			Logf("Binding %s to command: %s", keyName, command);
			BindCommand(it->second, command);
		}
		else
		{
			Errorf("Binding %s does not exist", keyName);
		}
	}
}
