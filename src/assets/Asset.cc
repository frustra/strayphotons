#include "assets/Asset.hh"

#include "assets/AssetManager.hh"

namespace sp {
    Asset::Asset(AssetManager *manager, const std::string &path, uint8 *buffer, size_t size)
        : manager(manager), path(path), buffer(buffer), size(size) {}

    Asset::~Asset() {
        manager->Unregister(*this);
        delete[] buffer;
    }

    const std::string Asset::String() {
        return std::string((char *)buffer, size);
    }

    const uint8 *Asset::Buffer() {
        return buffer;
    }

    const char *Asset::CharBuffer() {
        return (char *)buffer;
    }

    int Asset::Size() {
        return size;
    }
} // namespace sp
