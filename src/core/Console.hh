#pragma once

#include "CVar.hh"
#include "Logging.hh"

#include <map>
#include <mutex>
#include <queue>
#include <thread>

namespace sp
{
	struct ConsoleLine
	{
		logging::Level level;
		string text;
	};

	class ConsoleManager
	{
	public:
		ConsoleManager();
		void AddCVar(CVarBase *cvar);
		void Update();
		void InputLoop();

		void AddLog(logging::Level lvl, const string &line);

		const vector<ConsoleLine> Lines()
		{
			return outputLines;
		}

	private:
		void ParseAndExecute(const string &line);

		std::map<string, CVarBase *> cvars;
		std::queue<string> inputLines;
		std::mutex inputLock;
		std::thread inputThread;

		vector<ConsoleLine> outputLines;
	};

	extern ConsoleManager GConsoleManager;
}