#pragma once

#include "Common.hh"

namespace sp {
    class AssetManager;

    class Asset : public NonCopyable {
    public:
        Asset(AssetManager *manager, const string &path, uint8 *buffer, size_t size);
        ~Asset();

        const string String();
        const uint8 *Buffer();
        const char *CharBuffer();
        int Size();

        AssetManager *manager;

        const string path;

    private:
        uint8 *buffer = nullptr;
        size_t size = 0;
    };
} // namespace sp
