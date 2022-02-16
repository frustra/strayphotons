#pragma once

#include "core/Common.hh"

#include <functional>
#include <memory>

namespace sp {
    class Asset;

    /**
     * An Image asset is a texture that has been loaded into CPU memory.
     */
    class Image : public NonCopyable {
    public:
        Image(std::shared_ptr<const Asset> asset);

        int GetWidth() const {
            return width;
        }

        int GetHeight() const {
            return height;
        }

        int GetComponents() const {
            return components;
        }

        std::shared_ptr<const uint8_t> GetImage() const {
            return image;
        }

        size_t ByteSize() const {
            // stbi_load_from_memory returns 8 bits per channel
            return width * height * components;
        }

    private:
        int width, height, components;
        std::shared_ptr<const uint8_t> image;
    };
} // namespace sp
