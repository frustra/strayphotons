#include "Console.hh"
#include "Logging.hh"

#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <mutex>

namespace sp
{
	ConsoleManager GConsoleManager;

	namespace logging
	{
		void GlobalLogOutput(Level lvl, const string &line)
		{
			static std::mutex mut;
			mut.lock();
			GConsoleManager.AddLog(lvl, line);
			mut.unlock();
		}
	}

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

	void ConsoleManager::AddLog(logging::Level lvl, const string &line)
	{
		outputLines.push_back(
		{
			lvl,
			line
		});
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
		if (history.size() == 0 || history[history.size() - 1] != line)
			history.push_back(line);

		std::stringstream stream(line);
		string varName, value;
		stream >> varName;
		getline(stream, value);

		if (varName == "list")
		{
			for (auto &kv : cvars)
			{
				auto cvar = kv.second;
				logging::ConsoleWrite(logging::Level::Log, " > %s = %s", cvar->GetName(), cvar->StringValue());
				logging::ConsoleWrite(logging::Level::Log, " >   %s", cvar->GetDescription());
			}
			return;
		}

		auto cvarit = cvars.find(boost::algorithm::to_lower_copy(varName));
		if (cvarit != cvars.end())
		{
			auto cvar = cvarit->second;

			if (value.length() > 0)
			{
				cvar->SetFromString(value);
			}

			logging::ConsoleWrite(logging::Level::Log, " > %s = %s", cvar->GetName(), cvar->StringValue());

			if (value.length() == 0)
			{
				logging::ConsoleWrite(logging::Level::Log, " >   %s", cvar->GetDescription());
			}
		}
		else
		{
			logging::ConsoleWrite(logging::Level::Log, " > '%s' undefined", varName);
		}
	}

	string ConsoleManager::AutoComplete(const string &input)
	{
		auto it = cvars.upper_bound(boost::algorithm::to_lower_copy(input));
		if (it == cvars.end())
			return input;

		auto cvar = it->second;
		return cvar->GetName() + " ";
	}
}
