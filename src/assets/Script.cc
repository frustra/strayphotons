#include "Asset.hh"
#include "Script.hh"
#include <core/Console.hh>

#include <iostream>

namespace sp
{
	Script::Script(const string &path, shared_ptr<Asset> asset, vector<string> &&lines) : path(path), asset(asset), lines(std::move(lines))
	{
	}

    void Script::Exec()
    {
        Debugf("Running script: %s", path.c_str());
        for (string line : lines)
        {
            if (!line.empty() && line[0] != '#')
            {
                Debugf("$ %s", line.c_str());
                GetConsoleManager().ParseAndExecute(line);
            }
        }
    }
}