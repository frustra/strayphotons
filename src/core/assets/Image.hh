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
        Image() {}

        bool Valid() const {
            return valid.test();
        }

        void WaitUntilValid() const {
            valid.wait(false);
        }

        int GetWidth() const {
            Assert(valid.test(), "Accessing width on invalid image");
            return width;
        }

        int GetHeight() const {
            Assert(valid.test(), "Accessing height on invalid image");
            return height;
        }

        int GetComponents() const {
            Assert(valid.test(), "Accessing components on invalid image");
            return components;
        }

        std::shared_ptr<const uint8_t> GetImage() const {
            Assert(valid.test(), "Accessing data on invalid image");
            return image;
        }

    private:
        void PopulateFromAsset(std::shared_ptr<const Asset> asset);

        std::atomic_flag valid;
        int width, height, components;
        std::shared_ptr<const uint8_t> image;

        friend class AssetManager;
    };
} // namespace sp
