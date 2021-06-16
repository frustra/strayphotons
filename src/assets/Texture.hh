#pragma once

#include <functional>
#include <memory>

namespace sp {
    class Asset;
    class Texture;

    using TexturePtr = std::shared_ptr<Texture>;
    using StbImagePtr = std::shared_ptr<uint8_t[]>;

    /**
     * @brief A basic class that the core engine can use to work with texture data.
     *        This class is completely separate from any particular graphics pipeline,
     *        and should be able to be compiled into the headless build.
     */
    class Texture {
    public:
        // Factory function for loading a Texture from an Asset.
        // This is the primary way to construct a Texture.
        //
        // Note the use of move semantics for prevent accidentally copying
        // the texture around.
        static TexturePtr LoadFromAsset(std::shared_ptr<Asset> asset);

        // Destructor has to be public
        ~Texture() = default;

        int GetWidth() {
            return width;
        }
        int GetHeight() {
            return height;
        }
        int GetComponents() {
            return components;
        }
        StbImagePtr GetImage() {
            return image;
        }

        // Delete the copy constructor and assignment operator to force move semantics
        Texture(const Texture &src) = delete;
        void operator=(const Texture &src) = delete;

    private:
        // Private constructor forces use of factory functions
        Texture(StbImagePtr image, int width, int height, int components);

    private:
        int width, height, components;
        StbImagePtr image;
    };
} // namespace sp