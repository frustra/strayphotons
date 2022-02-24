#pragma once

#include "core/Common.hh"

#include <atomic>
#include <optional>
#include <vector>

namespace sp {
    class Asset : public NonCopyable {
    public:
        Asset(const std::string &path = "") : path(path) {}

        std::string String() const {
            return std::string((char *)buffer.data(), buffer.size());
        }

        const std::vector<uint8_t> &Buffer() const {
            return buffer;
        }

        const uint8_t *BufferPtr() const {
            return buffer.data();
        }

        const size_t BufferSize() const {
            return buffer.size();
        }

        Hash128 Hash() const;

        const std::string path;

    private:
        std::vector<uint8_t> buffer;
        std::optional<Hash128> hash;

        friend class AssetManager;
    };
} // namespace sp
