#pragma once

#include "CVar.hh"
#include <map>
#include <mutex>
#include <queue>
#include <thread>

namespace sp
{
	class ConsoleManager
	{
	public:
		ConsoleManager();
		void AddCVar(CVarBase *cvar);
		void Update();
		void InputLoop();

	private:
		void ParseAndExecute(const string &line);

		std::map<string, CVarBase *> cvars;
		std::queue<string> inputLines;
		std::mutex inputLock;
		std::thread inputThread;
	};

	extern ConsoleManager GConsoleManager;
}