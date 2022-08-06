#include "ConsoleScript.hh"

#include "assets/Asset.hh"
#include "console/Console.hh"

#include <iostream>
#include <sstream>

namespace sp {
    ConsoleScript::ConsoleScript(const string &path, shared_ptr<const Asset> asset) : path(path), asset(asset) {
        std::stringstream ss(asset->String());
        string line;
        while (std::getline(ss, line, '\n')) {
            trim(line);
            if (!line.empty() && line[0] != '#') lines.emplace_back(std::move(line));
        }
    }

    const std::vector<string> &ConsoleScript::Lines() const {
        return lines;
    }
} // namespace sp
