// Included directly into Console.cc

#include "Console.hh"
#include "Logging.hh"

namespace sp
{
	CFunc CFuncList("list", "Lists all CVar names, values, and descriptions", [](const string &s)
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
}
