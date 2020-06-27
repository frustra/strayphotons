#pragma once

#include "CVar.hh"
#include "CFunc.hh"
#include "Logging.hh"

#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

namespace sp
{
	class Script;

	struct ConsoleLine
	{
		logging::Level level;
		string text;
	};

	struct ConsoleInputLine
	{
		std::condition_variable handled;
		string text;
	};

	class ConsoleManager
	{
	public:
		ConsoleManager();
		void AddCVar(CVarBase *cvar);
		void RemoveCVar(CVarBase *cvar);
		void Update(Script *startupScript = nullptr);
		void InputLoop();

		void AddLog(logging::Level lvl, const string &line);

		const vector<ConsoleLine> Lines()
		{
			return outputLines;
		}

		const vector<string> History()
		{
			return history;
		}

		void ParseAndExecute(const string line, bool saveHistory = false);
		void Execute(const string cmd, const string &args);
		void QueueParseAndExecute(const string line, uint64 dt = 0);
		string AutoComplete(const string &input);
		vector<string> AllCompletions(const string &input);

		const std::map<string, CVarBase *> CVars()
		{
			return cvars;
		}

	private:
		std::map<string, CVarBase *> cvars;
		std::queue<ConsoleInputLine *> inputLines;
		std::mutex inputLock;
		std::thread inputThread;

		std::priority_queue<std::pair<uint64, string>> queuedCommands;
		vector<ConsoleLine> outputLines;
		vector<string> history;
	};

	ConsoleManager &GetConsoleManager();
}
