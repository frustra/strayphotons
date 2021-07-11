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
        Image(shared_ptr<Asset> asset);

        int GetWidth() {
            return width;
        }
        int GetHeight() {
            return height;
        }
        int GetComponents() {
            return components;
        }
        std::shared_ptr<uint8_t> GetImage() {
            return image;
        }

    private:
        int width, height, components;
        std::shared_ptr<uint8_t> image;
    };
} // namespace sp
