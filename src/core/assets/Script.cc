#include "Script.hh"

#include "Asset.hh"
#include "console/Console.hh"

#include <iostream>

namespace sp {
    Script::Script(const string &path, shared_ptr<const Asset> asset, vector<string> &&lines)
        : path(path), asset(asset), lines(std::move(lines)) {}

    void Script::Exec() {
        Debugf("Running script: %s", path.c_str());
        for (string line : lines) {
            if (!line.empty() && line[0] != '#') {
                Debugf("$ %s", line.c_str());
                GetConsoleManager().ParseAndExecute(line);
            }
        }
    }
} // namespace sp
