#pragma once

#include "core/Common.hh"

namespace sp {
    class Asset;

    class Script : public NonCopyable {
    public:
        Script(const string &path, shared_ptr<const Asset> asset, vector<string> &&lines);
        ~Script() {}

        void Exec();

        const string path;

    private:
        shared_ptr<const Asset> asset;
        vector<string> lines;
    };
} // namespace sp
