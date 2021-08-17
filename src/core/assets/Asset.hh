#pragma once

#include "core/Common.hh"

#include <vector>

namespace sp {
    class AssetManager;

    class Asset : public NonCopyable {
    public:
        Asset(const string &path) : path(path) {}

        std::string String() const {
            return std::string((char *)buffer.data(), buffer.size());
        }

        const char *CharBuffer() const {
            return (char *)buffer.data();
        }

        const string path;
        std::vector<uint8_t> buffer;
    };
} // namespace sp
