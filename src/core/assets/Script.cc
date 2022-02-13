#include "Script.hh"

#include "Asset.hh"
#include "console/Console.hh"

#include <iostream>
#include <sstream>

namespace sp {
    Script::Script(const string &path, shared_ptr<const Asset> asset) : path(path), asset(asset) {
        std::stringstream ss(asset->String());
        string line;
        while (std::getline(ss, line, '\n')) {
            if (!line.empty() && line[0] != '#') lines.emplace_back(std::move(line));
        }
    }

    const std::vector<string> &Script::Lines() const {
        return lines;
    }
} // namespace sp
