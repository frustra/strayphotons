#pragma once

#include "core/Common.hh"

namespace sp {
    class Asset;

    class ConsoleScript : public NonCopyable {
    public:
        ConsoleScript(const string &path, shared_ptr<const Asset> asset);

        const std::vector<string> &Lines() const;

        const string path;

    private:
        shared_ptr<const Asset> asset;
        vector<string> lines;
    };
} // namespace sp
