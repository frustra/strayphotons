#include "Asset.hh"
#include "Script.hh"
#include <core/Console.hh>

#include <iostream>

namespace sp
{
	Script::Script(const string &path, shared_ptr<Asset> asset, vector<string> &&lines) : path(path), asset(asset), lines(std::move(lines))
	{
	}

    bool Script::Exec()
    {
        for (string line : lines)
        {
            Debugf("Running script: %s", line.c_str());
			GetConsoleManager().ParseAndExecute(line);
        }
        return false;
    }
}
