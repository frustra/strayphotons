#include "Console.hh"
#include "Logging.hh"

#ifndef _WIN32
#define USE_LINENOISE_CLI
#endif

#ifdef USE_LINENOISE_CLI
#include <linenoise/linenoise.h>
#endif

#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>

namespace sp
{
	
	ConsoleManager &GetConsoleManager()
	{
		static ConsoleManager GConsoleManager;
		return GConsoleManager;
	}

#ifdef USE_LINENOISE_CLI
	void LinenoiseCompletionCallback(const char *buf, linenoiseCompletions *lc);
#endif

	namespace logging
	{
		void GlobalLogOutput(Level lvl, const string &line)
		{
			static std::mutex mut;
			mut.lock();
			GetConsoleManager().AddLog(lvl, line);
			mut.unlock();
		}
	}

	static uint64 NowMonotonicMs()
	{
		auto now = std::chrono::steady_clock::now();
		auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
		return now_ms.time_since_epoch().count();
	}

	CVarBase::CVarBase(const string &name, const string &description)
		: name(name), description(description)
	{
		GetConsoleManager().AddCVar(this);
	}

	CVarBase::~CVarBase()
	{
		GetConsoleManager().RemoveCVar(this);
	}

	ConsoleManager::ConsoleManager()
	{
		inputThread = std::thread([&] { this->InputLoop(); });
		inputThread.detach();
	}

	void ConsoleManager::AddCVar(CVarBase *cvar)
	{
		cvars[to_lower_copy(cvar->GetName())] = cvar;
	}

	void ConsoleManager::RemoveCVar(CVarBase *cvar)
	{
		cvars.erase(to_lower_copy(cvar->GetName()));
	}

	void ConsoleManager::InputLoop()
	{
		std::unique_lock<std::mutex> ulock(inputLock, std::defer_lock);

#ifdef USE_LINENOISE_CLI
		char *str;

		linenoiseHistorySetMaxLen(256);
		linenoiseSetCompletionCallback(LinenoiseCompletionCallback);

		while ((str = linenoise("sp> ")) != nullptr)
		{
			if (*str == '\0')
			{
				linenoiseFree(str);
				continue;
			}

			ConsoleInputLine line;
			line.text = string(str);

			ulock.lock();
			inputLines.push(&line);
			line.handled.wait(ulock);
			ulock.unlock();

			linenoiseHistoryAdd(str);
			linenoiseFree(str);
		}
#else
		std::string str;

		while (std::getline(std::cin, str))
		{
			if (str == "")
				continue;

			ConsoleInputLine line;
			line.text = str;

			ulock.lock();
			inputLines.push(&line);
			line.handled.wait(ulock);
			ulock.unlock();
		}
#endif
	}

	void ConsoleManager::AddLog(logging::Level lvl, const string &line)
	{
		outputLines.push_back(
		{
			lvl,
			line
		});
	}

	void ConsoleManager::Update(Script *startupScript)
	{
		std::unique_lock<std::mutex> ulock(inputLock, std::defer_lock);

		uint64 now = NowMonotonicMs();

		if (startupScript != nullptr && queuedCommands.empty())
		{
			ParseAndExecute("exit");
		}
		while (!queuedCommands.empty())
		{
			auto top = queuedCommands.top();
			if (top.first > now)
				break;

			queuedCommands.pop();
			ParseAndExecute(top.second, false);
		}

		while (!inputLines.empty())
		{
			if (!ulock.try_lock())
				return;

			auto line = inputLines.front();
			inputLines.pop();
			ParseAndExecute(line->text, true);
			ulock.unlock();

			line->handled.notify_all();
		}
	}

	void ConsoleManager::ParseAndExecute(const string line, bool saveHistory)
	{
		if (line == "")
			return;

		if (saveHistory && (history.size() == 0 || history[history.size() - 1] != line))
			history.push_back(line);

		auto cmd = line.begin();
		do
		{
			auto cmdEnd = std::find(cmd, line.end(), ';');

			std::stringstream stream(std::string(cmd, cmdEnd));
			string varName, value;
			stream >> varName;
			getline(stream, value);
			trim(value);
			Execute(varName, value);

			if (cmdEnd != line.end()) cmdEnd++;
			cmd = cmdEnd;
		}
		while (cmd != line.end());
	}

	void ConsoleManager::Execute(const string cmd, const string &args)
	{
		auto cvarit = cvars.find(to_lower_copy(cmd));
		if (cvarit != cvars.end())
		{
			auto cvar = cvarit->second;
			cvar->SetFromString(args);

			if (cvar->IsValueType())
			{
				logging::ConsoleWrite(logging::Level::Log, " > %s = %s", cvar->GetName(), cvar->StringValue());

				if (args.length() == 0)
				{
					logging::ConsoleWrite(logging::Level::Log, " >   %s", cvar->GetDescription());
				}
			}
		}
		else
		{
			logging::ConsoleWrite(logging::Level::Log, " > '%s' undefined", cmd);
		}
	}

	void ConsoleManager::QueueParseAndExecute(const string line, uint64 dt)
	{
		// Queue commands 1ms forward so they don't run same frame.
		queuedCommands.push({NowMonotonicMs() + dt + 1, line});
	}

	string ConsoleManager::AutoComplete(const string &input)
	{
		auto it = cvars.upper_bound(to_lower_copy(input));
		if (it == cvars.end())
			return input;

		auto cvar = it->second;
		return cvar->GetName() + " ";
	}

	vector<string> ConsoleManager::AllCompletions(const string &rawInput)
	{
		vector<string> results;

		auto input = to_lower_copy(rawInput);
		auto it = cvars.lower_bound(input);

		for (; it != cvars.end(); it++)
		{
			auto cvar = it->second;
			auto name = cvar->GetName();

			if (starts_with(to_lower_copy(name), input))
				results.push_back(name);
			else
				break;
		}

		return results;
	}

#ifdef USE_LINENOISE_CLI
	void LinenoiseCompletionCallback(const char *buf, linenoiseCompletions *lc)
	{
		auto completions = GetConsoleManager().AllCompletions(string(buf));
		for (auto str : completions)
		{
			linenoiseAddCompletion(lc, str.c_str());
		}
	}
#endif
}

#include "ConsoleCoreCommands.hh"
