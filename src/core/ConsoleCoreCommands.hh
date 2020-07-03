// Included directly into Console.cc

#include "Console.hh"
#include "Logging.hh"

#include <sstream>

namespace sp
{
	CFunc<void> CFuncList("list", "Lists all CVar names, values, and descriptions", []()
	{
		for (auto &kv : GetConsoleManager().CVars())
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
		GetConsoleManager().QueueParseAndExecute(cmd, chrono_clock::now() + std::chrono::milliseconds(dt));
	});

	CFunc<string> CFuncToggle("toggle", "Toggle a CVar between values (toggle <cvar_name> [<value_a> <value_b>])", [](string args)
	{
		std::stringstream stream(args);
		string cvarName;
		stream >> cvarName;

		auto cvars = GetConsoleManager().CVars();
		auto cvarit = cvars.find(to_lower_copy(cvarName));
		if (cvarit != cvars.end())
		{
			auto cvar = cvarit->second;
			if (cvar->IsValueType())
			{
				vector<string> values;
				string value;
				while (stream >> value)
				{
					values.push_back(value);
				}
				cvar->ToggleValue(values.data(), values.size());
			}
			else
			{
				logging::ConsoleWrite(logging::Level::Log, " > '%s' is not a cvar", cvarName);
			}
		}
		else
		{
			logging::ConsoleWrite(logging::Level::Log, " > '%s' undefined", cvarName);
		}
	});
}
