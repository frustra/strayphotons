#pragma once

#include "core/Common.hh"

namespace sp {
    class Asset;

    class Script : public NonCopyable {
    public:
        Script(const string &path, shared_ptr<const Asset> asset);
        ~Script() {}

        const std::vector<string> &Lines() const;

        const string path;

    private:
        shared_ptr<const Asset> asset;
        vector<string> lines;
    };
} // namespace sp
