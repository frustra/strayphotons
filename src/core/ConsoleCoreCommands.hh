// Included directly into Console.cc

#include "Console.hh"
#include "Logging.hh"

#include <sstream>

namespace sp
{
	CFunc<void> CFuncList("list", "Lists all CVar names, values, and descriptions", []()
	{
		for (auto &kv : GConsoleManager.CVars())
		{
			auto cvar = kv.second;
			if (cvar->IsValueType())
			{
				logging::ConsoleWrite(logging::Level::Log, " > %s = %s", cvar->GetName(), cvar->StringValue());
			}
			else
			{
				logging::ConsoleWrite(logging::Level::Log, " > %s (func)", cvar->GetName());
			}

			auto desc = cvar->GetDescription();
			if (desc.size() > 0)
			{
				logging::ConsoleWrite(logging::Level::Log, " >   %s", desc);
			}
		}
	});

	CFunc<string> CFuncWait("wait", "Queue command for later (wait <ms> <command>)", [](string args)
	{
		std::stringstream stream(args);
		uint64 dt;
		stream >> dt;

		string cmd;
		getline(stream, cmd);
		GConsoleManager.QueueParseAndExecute(cmd, dt);
	});
}
