#include "Console.hh"

#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>

namespace sp
{
	ConsoleManager GConsoleManager;

	CVarBase::CVarBase(const string &name, const string &description)
		: name(name), description(description)
	{
		GConsoleManager.AddCVar(this);
	}

	ConsoleManager::ConsoleManager()
	{
		inputThread = std::thread([&] { this->InputLoop(); });
		inputThread.detach();
	}

	void ConsoleManager::AddCVar(CVarBase *cvar)
	{
		cvars[boost::algorithm::to_lower_copy(cvar->GetName())] = cvar;
	}

	void ConsoleManager::InputLoop()
	{
		string line;

		while (std::getline(std::cin, line))
		{
			inputLock.lock();
			inputLines.push(line);
			inputLock.unlock();
		}
	}

	void ConsoleManager::Update()
	{
		if (!inputLock.try_lock())
			return;

		while (!inputLines.empty())
		{
			string line = inputLines.back();
			inputLines.pop();

			ParseAndExecute(line);
		}

		inputLock.unlock();
	}

	void ConsoleManager::ParseAndExecute(const string &line)
	{
		std::stringstream stream(line);
		string varName, value;
		stream >> varName >> value;

		if (varName == "list")
		{
			for (auto & kv : cvars)
			{
				auto cvar = kv.second;
				std::cout << " > " << cvar->GetName() << " " << cvar->StringValue() << std::endl;
				std::cout << " >   " << cvar->GetDescription() << std::endl;
			}
			return;
		}

		auto cvar = cvars[boost::algorithm::to_lower_copy(varName)];
		if (cvar)
		{
			if (value.length() > 0)
			{
				cvar->SetFromString(value);
			}

			std::cout << " > " << cvar->GetName() << " " << cvar->StringValue() << std::endl;

			if (value.length() == 0)
			{
				std::cout << " >   " << cvar->GetDescription() << std::endl;
			}
		}
		else
		{
			std::cout << " > " << "CVar '" << varName << "' undefined" << std::endl;
		}
	}
}
