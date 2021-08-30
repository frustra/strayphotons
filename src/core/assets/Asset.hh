#pragma once

#include "core/Common.hh"

#include <atomic>
#include <vector>

namespace sp {
    class Asset : public NonCopyable {
    public:
        Asset(const std::string &path = "") : path(path) {}

        bool Valid() const {
            return valid.test();
        }

        void WaitUntilValid() const {
            while (!valid.test()) {
                valid.wait(false);
            }
        }

        std::string String() const {
            Assert(valid.test(), "Accessing buffer string on invalid asset");
            return std::string((char *)buffer.data(), buffer.size());
        }

        const uint8_t *Buffer() const {
            Assert(valid.test(), "Accessing buffer data on invalid asset");
            return buffer.data();
        }

        const size_t BufferSize() const {
            Assert(valid.test(), "Accessing buffer size on invalid asset");
            return buffer.size();
        }

        Hash128 Hash() const;

        const std::string path;

    private:
        std::atomic_flag valid;
        std::vector<uint8_t> buffer;

        friend class AssetManager;
    };
} // namespace sp
